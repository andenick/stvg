#pragma once

#include "BankState.h"
#include <cmath>
#include <algorithm>

namespace stvg {

struct RiskWeightTable {
    static double weight(DivisionType type, int year) {
        switch (type) {
            case DivisionType::CommercialLending:    return 1.00;
            case DivisionType::MortgageLending:      return 0.50;  // GSE-backed
            case DivisionType::TrustAndCustody:      return 0.20;  // off-balance-sheet
            case DivisionType::CreditCards:          return 0.75;
            case DivisionType::InternationalBanking: return 1.00;
            case DivisionType::TradingDesk:          return 0.50;
            case DivisionType::AssetManagement:      return 0.20;
            case DivisionType::InvestmentBanking:    return 0.50;
            case DivisionType::PrivateEquity:        return 1.50;
            case DivisionType::Restructuring:        return 0.50;
            case DivisionType::Securitization:
                return (year < 2010) ? 0.20 : 1.00;  // pre-Basel III loophole
            case DivisionType::DerivativesDesk:
                return (year < 2010) ? 0.50 : 1.50;
            case DivisionType::WealthManagement:     return 0.20;
            case DivisionType::VentureCapital:       return 1.50;
            case DivisionType::Fintech:              return 0.75;
            case DivisionType::CryptoCustody:        return 1.00;
        }
        return 1.00;
    }

    static double computeRWA(const Bank& bank, int year) {
        double rwa = 0;
        for (const auto& div : bank.divisions) {
            double assetValue = div.budget * Bank::kDefaultLeverage;
            rwa += assetValue * weight(div.type, year);
        }
        rwa += bank.reserves * 0.0; // cash/reserves = 0% weight
        return std::max(rwa, 1.0);
    }

    static double cet1Ratio(const Bank& bank, int year) {
        double rwa = computeRWA(bank, year);
        return bank.capital / rwa;
    }

    static double requiredCET1(int year) {
        if (year < 1988) return 0.0;      // pre-Basel I: no requirement
        if (year < 2004) return 0.04;      // Basel I: 4%
        if (year < 2010) return 0.04;      // Basel II: 4% (but internal models)
        if (year < 2019) return 0.045;     // Basel III phase-in
        return 0.08;                        // Basel III + buffers
    }

    static double requiredWithBuffers(int year) {
        double base = requiredCET1(year);
        if (year >= 2016) base += 0.025;   // capital conservation buffer
        if (year >= 2019) base += 0.01;    // countercyclical buffer (average)
        return base;
    }
};

} // namespace stvg
