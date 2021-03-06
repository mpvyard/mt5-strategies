﻿using FileHelpers;

namespace TestingResultsAnalyzer
{
    public class DirectionConverter : ConverterBase
    {
        public override object StringToField(string from)
        {
            return from == "L" ? TradeDirection.Long : TradeDirection.Short;
        }
    }
}