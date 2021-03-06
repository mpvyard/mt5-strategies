﻿using TestingResultsAnalyzer.ViewModels;

namespace TestingResultsAnalyzer.Filters
{
    public class H4RsiFilter : Filter
    {
        public H4RsiFilter()
        {
            ArgumentValue = "30";
        }

        public override string Name => "H4 RSI";

        public override string Description => "For long trades, only enter when 4 hourly RSI <= 30.  For short trades, only enter when 4 hourly RSI >= 70";

        public override bool IsCombinable => true;

        public override bool HasArgument => true;

        public override bool IsIncluded(TradeViewModel trade)
        {
            if (!int.TryParse(ArgumentValue, out int value))
            {
                return false;
            }

            return trade.Direction == TradeDirection.Long
                ? trade.H4Rsi <= value
                : trade.H4Rsi >= (100 - value);
        }
    }
}
