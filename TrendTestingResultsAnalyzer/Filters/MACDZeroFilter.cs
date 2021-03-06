﻿using TrendTestingResultsAnalyzer.ViewModels;

namespace TrendTestingResultsAnalyzer.Filters
{
    public class MACDZeroFilter : Filter
    {
        public override string Name => "MACD Zero Line";

        public override string Description => "Only take trades when the MACD is below the zero line (or above the zero line when going short)";

        public override bool IsCombinable => true;

        public override bool IsIncluded(TradeViewModel trade)
        {
            return trade.Direction == TradeDirection.Long
                ? trade.MACD < 0
                : trade.MACD > 0;
        }
    }
}
