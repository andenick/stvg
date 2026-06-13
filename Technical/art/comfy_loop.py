#!/usr/bin/env python3
# =============================================================================
# comfy_loop.py — minimal ComfyUI HTTP client for the STVG art pipeline.
#
# *** UNTESTED BY DESIGN. NOTHING HERE HAS RUN. ***
# This file is a SPEC you can read before any GPU/ComfyUI software exists on the
# box. It is written to be correct against ComfyUI's documented HTTP API, but it
# has NOT been executed against a live server — the first run is session A0/E0,
# and only on the owner's go-ahead (see ../Plans/ART_EXPLORATION_PLAN.md).
#
# Dependencies: Python standard library ONLY (urllib, json, argparse, ...).
# No torch, no requests, no ComfyUI import — this is a thin HTTP client so it
# stays readable and dependency-free.
#
# The loop it serves (Claude drives the judgement steps in conversation):
#   load workflow JSON -> mutate node inputs by TITLE -> POST /prompt
#     -> poll /history/{id} -> download PNGs -> *Claude inspects* -> repeat.
#
# Node TITLES, not numeric ids, are the stable handle for mutation: every
# editable node in a shipped workflow template carries a SCREAMING_SNAKE
# `_meta.title` (e.g. POSITIVE_PROMPT, SEED, LORA_STRENGTH). See
# workflows/README_workflows.md.
# =============================================================================
from __future__ import annotations

import argparse
import json
import time
import urllib.error
import urllib.parse
import urllib.request
import uuid
from pathlib import Path
from typing import Any

DEFAULT_HOST = "127.0.0.1:8188"
DEFAULT_POLL_SECONDS = 1.5
DEFAULT_TIMEOUT_SECONDS = 600  # a single SDXL/Kontext render should finish well inside this


# --------------------------------------------------------------------------- #
# Workflow type alias. A ComfyUI "prompt" is a dict keyed by node-id string,
# each value: {"class_type": str, "inputs": {...}, "_meta": {"title": str}}.
# --------------------------------------------------------------------------- #
Workflow = dict[str, dict[str, Any]]


class ComfyClient:
    """Thin client over the ComfyUI HTTP API (submit / poll / fetch)."""

    def __init__(self, host: str = DEFAULT_HOST, client_id: str | None = None) -> None:
        self.base = f"http://{host}"
        self.client_id = client_id or str(uuid.uuid4())

    # ---- low-level HTTP helpers (stdlib urllib) --------------------------- #
    def _get_json(self, path: str) -> Any:
        req = urllib.request.Request(self.base + path, method="GET")
        with urllib.request.urlopen(req, timeout=30) as resp:
            return json.loads(resp.read().decode("utf-8"))

    def _post_json(self, path: str, payload: dict[str, Any]) -> Any:
        data = json.dumps(payload).encode("utf-8")
        req = urllib.request.Request(
            self.base + path, data=data, method="POST",
            headers={"Content-Type": "application/json"},
        )
        with urllib.request.urlopen(req, timeout=30) as resp:
            return json.loads(resp.read().decode("utf-8"))

    def _get_bytes(self, path: str) -> bytes:
        req = urllib.request.Request(self.base + path, method="GET")
        with urllib.request.urlopen(req, timeout=60) as resp:
            return resp.read()

    # ---- API surface ----------------------------------------------------- #
    def submit(self, workflow: Workflow) -> str:
        """POST a workflow dict to /prompt. Returns the prompt_id."""
        body = {"prompt": workflow, "client_id": self.client_id}
        out = self._post_json("/prompt", body)
        prompt_id = out.get("prompt_id")
        if not prompt_id:
            raise RuntimeError(f"/prompt returned no prompt_id: {out!r}")
        return prompt_id

    def history(self, prompt_id: str) -> dict[str, Any]:
        """GET /history/{id}. Empty dict until the run has completed."""
        return self._get_json(f"/history/{urllib.parse.quote(prompt_id)}")

    def wait(
        self,
        prompt_id: str,
        poll_seconds: float = DEFAULT_POLL_SECONDS,
        timeout_seconds: float = DEFAULT_TIMEOUT_SECONDS,
    ) -> dict[str, Any]:
        """Poll /history/{id} until the run appears, then return its record."""
        deadline = time.monotonic() + timeout_seconds
        while time.monotonic() < deadline:
            hist = self.history(prompt_id)
            if prompt_id in hist:
                return hist[prompt_id]
            time.sleep(poll_seconds)
        raise TimeoutError(f"prompt {prompt_id} did not complete in {timeout_seconds}s")

    @staticmethod
    def _iter_output_images(record: dict[str, Any]) -> list[dict[str, str]]:
        """Pull the {filename, subfolder, type} entries out of a history record."""
        images: list[dict[str, str]] = []
        for node_out in record.get("outputs", {}).values():
            for img in node_out.get("images", []) or []:
                images.append(img)
        return images

    def download_outputs(self, record: dict[str, Any], out_dir: str | Path) -> list[Path]:
        """Fetch every output image named in a history record to out_dir."""
        out_dir = Path(out_dir)
        out_dir.mkdir(parents=True, exist_ok=True)
        saved: list[Path] = []
        for img in self._iter_output_images(record):
            q = urllib.parse.urlencode({
                "filename": img.get("filename", ""),
                "subfolder": img.get("subfolder", ""),
                "type": img.get("type", "output"),
            })
            blob = self._get_bytes(f"/view?{q}")
            dest = out_dir / img.get("filename", f"out_{uuid.uuid4().hex}.png")
            dest.write_bytes(blob)
            saved.append(dest)
        return saved

    def run(
        self,
        workflow: Workflow,
        out_dir: str | Path,
        poll_seconds: float = DEFAULT_POLL_SECONDS,
        timeout_seconds: float = DEFAULT_TIMEOUT_SECONDS,
    ) -> list[Path]:
        """submit -> wait -> download. The whole one-shot generate step."""
        prompt_id = self.submit(workflow)
        record = self.wait(prompt_id, poll_seconds, timeout_seconds)
        return self.download_outputs(record, out_dir)


# --------------------------------------------------------------------------- #
# Mutate helpers. Locate a node by its `_meta.title` and set one input. These
# are deliberately small and side-effecting-on-a-copy is the caller's choice
# (use copy.deepcopy on the loaded template before mutating if you want to keep
# the original pristine).
# --------------------------------------------------------------------------- #
def find_node_by_title(workflow: Workflow, title: str) -> str:
    """Return the node-id whose _meta.title == title (raises if 0 or >1)."""
    matches = [
        nid for nid, node in workflow.items()
        if node.get("_meta", {}).get("title") == title
    ]
    if not matches:
        raise KeyError(f"no node titled {title!r} in workflow")
    if len(matches) > 1:
        raise KeyError(f"ambiguous: {len(matches)} nodes titled {title!r}")
    return matches[0]


def set_input(workflow: Workflow, title: str, key: str, value: Any) -> None:
    """Set workflow[node].inputs[key] = value for the node titled `title`."""
    nid = find_node_by_title(workflow, title)
    workflow[nid].setdefault("inputs", {})[key] = value


def set_prompt(workflow: Workflow, text: str, title: str = "POSITIVE_PROMPT") -> None:
    """Set a CLIP text-encode / prompt node's text. Default title POSITIVE_PROMPT."""
    set_input(workflow, title, "text", text)


def set_negative(workflow: Workflow, text: str, title: str = "NEGATIVE_PROMPT") -> None:
    set_input(workflow, title, "text", text)


def set_seed(workflow: Workflow, seed: int, title: str = "SEED") -> None:
    """Set the sampler seed. Some sampler nodes call the input `seed`, others
    `noise_seed`; we set whichever the node exposes (try both safely)."""
    nid = find_node_by_title(workflow, title)
    inputs = workflow[nid].setdefault("inputs", {})
    if "noise_seed" in inputs:
        inputs["noise_seed"] = seed
    else:
        inputs["seed"] = seed


def set_lora_strength(
    workflow: Workflow, strength: float, title: str = "LORA_STRENGTH",
) -> None:
    """Set a LoraLoader's model strength (and clip strength to match)."""
    nid = find_node_by_title(workflow, title)
    inputs = workflow[nid].setdefault("inputs", {})
    inputs["strength_model"] = strength
    if "strength_clip" in inputs:
        inputs["strength_clip"] = strength


def set_denoise(workflow: Workflow, denoise: float, title: str = "SAMPLER") -> None:
    """Set the KSampler denoise (used by the Kontext edit graph for edit strength)."""
    set_input(workflow, title, "denoise", denoise)


def load_workflow(path: str | Path) -> Workflow:
    """Read a workflow template JSON from disk."""
    return json.loads(Path(path).read_text(encoding="utf-8"))


# --------------------------------------------------------------------------- #
# CLI. Smoke-test entry point for A0/E0: load a template, optionally override
# prompt/seed, run it, report where the PNGs landed. (Will only work once a
# ComfyUI server is actually up — see the header.)
# --------------------------------------------------------------------------- #
def _build_arg_parser() -> argparse.ArgumentParser:
    p = argparse.ArgumentParser(description="ComfyUI submit/poll/fetch smoke client (untested).")
    p.add_argument("workflow", help="path to a workflow template JSON")
    p.add_argument("--host", default=DEFAULT_HOST, help=f"ComfyUI host (default {DEFAULT_HOST})")
    p.add_argument("--out", default="./_out", help="output directory for PNGs")
    p.add_argument("--prompt", default=None, help="override POSITIVE_PROMPT text")
    p.add_argument("--seed", type=int, default=None, help="override SEED")
    p.add_argument("--poll", type=float, default=DEFAULT_POLL_SECONDS, help="poll interval (s)")
    p.add_argument("--timeout", type=float, default=DEFAULT_TIMEOUT_SECONDS, help="overall timeout (s)")
    return p


def main(argv: list[str] | None = None) -> int:
    args = _build_arg_parser().parse_args(argv)
    wf = load_workflow(args.workflow)
    if args.prompt is not None:
        set_prompt(wf, args.prompt)
    if args.seed is not None:
        set_seed(wf, args.seed)
    client = ComfyClient(host=args.host)
    saved = client.run(wf, args.out, poll_seconds=args.poll, timeout_seconds=args.timeout)
    for path in saved:
        print(f"saved: {path}")
    if not saved:
        print("WARNING: run completed but produced no images — check the workflow graph.")
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
