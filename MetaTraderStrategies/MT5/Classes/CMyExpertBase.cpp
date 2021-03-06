#include <Trade\Trade.mqh>
#include <Trade\SymbolInfo.mqh> 
#include <Trade\PositionInfo.mqh>
#include <Expert\Money\MoneyFixedRisk.mqh>

enum STOPLOSS_RULE
{
    None,
    StaticPipsValue,
    CurrentBar2Pips,
    CurrentBar2ATR,
    PreviousBar5Pips,
    PreviousBar2Pips,
    CurrentBar5Pips,
    CurrentBarNPips,
    ShortTermHighLow
};

enum LOTSIZING_RULE
{
    Static,
    Dynamic
};

//--- constants of identification of trend
enum TREND_TYPE
{
    TREND_TYPE_HARD_DOWN = 0,   // strong down trend
    TREND_TYPE_DOWN = 1,        // down trend
    TREND_TYPE_SOFT_DOWN = 2,   // weak down trend
    TREND_TYPE_FLAT = 3,        // no trend
    TREND_TYPE_SOFT_UP = 4,     // weak up trend
    TREND_TYPE_UP = 5,          // up trend
    TREND_TYPE_HARD_UP = 6      // strong up trend
};

class CMyExpertBase
{
public:
    CMyExpertBase(void);
    ~CMyExpertBase(void);

    virtual int Init
    (
        LOTSIZING_RULE  inpLotSizingRule,
        double          inpDynamicSizingRiskPerTrade,
        double          inpLots,
        STOPLOSS_RULE   inpInitialStopLossRule,
        int             inpInitialStopLossPips,
        bool            inpUseTakeProfit,
        int             inpTakeProfitPips,
        double          inpTakeProfitRiskRewardRatio,
        STOPLOSS_RULE   inpTrailingStopLossRule,
        int             inpTrailingStopPips,        
        bool            inpMoveToBreakEven,
        bool            inpGoLong,
        bool            inpGoShort,
        bool            inpAlertTerminalEnabled,
        bool            inpAlertEmailEnabled,
        int             inpMinutesToWaitAfterPositionClosed,
        int             inpMinTradingHour,
        int             inpMaxTradingHour
    );
    virtual void              Deinit(void);
    virtual void              Processing(void);
    virtual void              OnTrade(void);
    virtual bool              HasBullishSignal();
    virtual bool              HasBearishSignal();

protected:
    CSymbolInfo _symbol;
    CPositionInfo _position;
    CTrade _trade;
    MqlRates _prices[];
    int _digits_adjust;
    double _adjustedPoints;
    double _trailing_stop;
    double _currentBid, _currentAsk;
    double _atrData[];

    LOTSIZING_RULE _inpLotSizingRule;
    double _inpLots;
    STOPLOSS_RULE _inpInitialStopLossRule;
    int _inpInitialStopLossPips;
    bool _inpUseTakeProfit;
    int _inpTakeProfitPips;
    double _inpTakeProfitRiskRewardRatio;
    STOPLOSS_RULE _inpTrailingStopLossRule;
    bool _inpGoLong;
    bool _inpGoShort;
    bool _inpAlertTerminalEnabled;
    bool _inpAlertEmailEnabled;
    int _inpMinutesToWaitAfterPositionClosed;
    int _inpMinTradingHour;
    int _inpMaxTradingHour;
    bool _inpMoveToBreakEven;
    bool _alreadyMovedToBreakEven;
    double _initialStop;
    bool _trailingStarted;
    double _recentTurningPoint;
    bool _hadRecentTurningPoint;

    virtual bool CheckToModifyPositions();
    void ReleaseIndicator(int& handle);
    virtual void NewBarAndNoCurrentPositions();
    virtual void OnRecentlyClosedTrade();
    bool IsNewBar(datetime currentTime);
    datetime iTime(const int index, string symbol = NULL, ENUM_TIMEFRAMES timeframe = PERIOD_CURRENT);

private:
    void ResetState();
    bool RefreshRates();    
    void OpenPosition(string symbol, ENUM_ORDER_TYPE orderType, double volume, double price, double stopLoss, double takeProfit);
    bool LongModified();
    bool ShortModified();
    bool IsOutsideTradingHours();
    double CalculateStopLossLevelForBuyOrder();
    double CalculateStopLossLevelForSellOrder();
    bool ShouldPauseUntilOpeningNewPosition();    

    datetime _barTime;                  // For detection of a new bar
    double _recentHigh;                 // Tracking the most recent high for stop management
    double _recentLow;                  // Tracking the most recent low for stop management
    int _atrHandle;                     // Average True Range indicator handle
    int _barsSincePositionOpened;       // Counter of the number of bars since a position was opened
    int _previousOrderTotal;            // Number of orders at the time of previous OnTrade() call
    int _GetLastError;                  // Error code
    ulong _lastOrderTicket;             // Ticket of the last processed order
    int _eventCount;                    // Counter for OnTrade event
    int _currentPositionType;           // The current type of position (long/short)
    datetime _lastClosedPositionTime;   // The time when a position was most recently closed
    CMoneyFixedRisk _fixedRisk;         // Fixed risk money management class
};

CMyExpertBase::CMyExpertBase(void)
{
}

CMyExpertBase::~CMyExpertBase(void)
{
}

int CMyExpertBase::Init(
    LOTSIZING_RULE  inpLotSizingRule,
    double          inpDynamicSizingRiskPerTrade,
    double          inpLots,
    STOPLOSS_RULE   inpInitialStopLossRule,
    int             inpInitialStopLossPips,
    bool            inpUseTakeProfit,
    int             inpTakeProfitPips,
    double          inpTakeProfitRiskRewardRatio,
    STOPLOSS_RULE   inpTrailingStopLossRule,
    int             inpTrailingStopPips,
    bool            inpMoveToBreakEven,
    bool            inpGoLong,
    bool            inpGoShort,
    bool            inpAlertTerminalEnabled,
    bool            inpAlertEmailEnabled,
    int             inpMinutesToWaitAfterPositionClosed,
    int             inpMinTradingHour,
    int             inpMaxTradingHour    
)
{
    if (!_symbol.Name(Symbol())) // sets symbol name
        return(INIT_FAILED);

    if (!RefreshRates()) {
        Print("Could not refresh rates - init failed.");
        return(INIT_FAILED);
    }
    
    if (inpLotSizingRule == Dynamic && inpLots != 0) {
        Print("Invalid lot size - must be 0 when using dynamic sizing - init failed.");
        return(INIT_FAILED);
    }

    if ((inpLotSizingRule == Static) && !(inpLots > 0 && inpLots <= 10)) {
        Print("Invalid lot size - init failed.");
        return(INIT_FAILED);
    }

    if (inpInitialStopLossRule != StaticPipsValue && inpInitialStopLossRule != CurrentBarNPips && inpInitialStopLossPips != 0) {
        Print("Invalid initial stop loss rule.  Pips should be 0 when not using StaticPipsValue or CurrentBarNPips - init failed.");
        return(INIT_FAILED);
    }

    if ((inpInitialStopLossRule == StaticPipsValue || inpInitialStopLossRule == CurrentBarNPips) && inpInitialStopLossPips <= 0) {
        Print("Invalid initial stop loss pip value.  Pips should be greater than 0 - init failed.");
        return(INIT_FAILED);
    }

    if (inpTrailingStopLossRule != StaticPipsValue && inpTrailingStopPips != 0) {
        Print("Invalid trailing stop loss rule.  Pips should be 0 when not using StaticPipsValue - init failed.");
        return(INIT_FAILED);
    }

    if (inpTrailingStopLossRule == StaticPipsValue && inpTrailingStopPips <= 0) {
        Print("Invalid trailing stop loss pip value.  Pips should be greater than 0 - init failed.");
        return(INIT_FAILED);
    }

    if (inpUseTakeProfit && inpTakeProfitPips <= 0 && inpTakeProfitRiskRewardRatio <= 0) {
        Print("Invalid take profit pip value / risk reward ratio.  Pips or risk reward ratio should be greater than 0 - init failed.");
        return(INIT_FAILED);
    }

    if (inpUseTakeProfit && inpTakeProfitPips > 0 && inpTakeProfitRiskRewardRatio > 0) {
        Print("Invalid take profit parameters.  Can use only one of either take profit pip value or risk reward ratio - init failed.");
        return(INIT_FAILED);
    }

    if (!inpUseTakeProfit && inpTakeProfitPips != 0) {
        Print("Invalid take profit pip value.  Pips should be 0 when not using take profit - init failed.");
        return(INIT_FAILED);
    }

    if (!inpUseTakeProfit && inpTakeProfitRiskRewardRatio != 0) {
        Print("Invalid risk/reward ratio value.  Value should be 0 when not using take profit - init failed.");
        return(INIT_FAILED);
    }

    if (inpMinutesToWaitAfterPositionClosed < 0) {
        Print("Invalid number of minutes to wait after position closed. Value should be >= 0 - init failed.");
        return(INIT_FAILED);
    }

    if (inpMinTradingHour < 0 || inpMinTradingHour > 23) {
        Print("Invalid min trading hour. Value should be between 0 and 23 - init failed.");
        return(INIT_FAILED);
    }

    if (inpMaxTradingHour < 0 || inpMaxTradingHour > 23) {
        Print("Invalid max trading hour. Value should be between 0 and 23 - init failed.");
        return(INIT_FAILED);
    }

    if (inpMaxTradingHour < inpMinTradingHour) {
        Print("Invalid min/max trading hours. Min should be less than or equal to max - init failed.");
        return(INIT_FAILED);
    }

    if (inpInitialStopLossRule == None && inpTrailingStopLossRule == None) {
        Print("Invalid stop loss rules - both initial and trailing are set to None - init failed.");
        return(INIT_FAILED);
    }

    ArraySetAsSeries(_prices, true);
    ArraySetAsSeries(_atrData, true);
    _atrHandle = iATR(_Symbol, 0, 14);

    _digits_adjust = 1;
    if (_Digits == 5 || _Digits == 3 || _Digits == 1) {
        _digits_adjust = 10;
    }

    _adjustedPoints = _symbol.Point() * _digits_adjust;

    _inpLotSizingRule = inpLotSizingRule;
    _inpLots = inpLots;
    _inpInitialStopLossRule = inpInitialStopLossRule;
    _inpInitialStopLossPips = inpInitialStopLossPips;
    _inpUseTakeProfit = inpUseTakeProfit;
    _inpTakeProfitPips = inpTakeProfitPips;
    _inpTakeProfitRiskRewardRatio = inpTakeProfitRiskRewardRatio;
    _inpTrailingStopLossRule = inpTrailingStopLossRule;
    _inpGoLong = inpGoLong;
    _inpGoShort = inpGoShort;

    _trailing_stop = inpTrailingStopPips * _adjustedPoints;

    _inpAlertTerminalEnabled = inpAlertTerminalEnabled;
    _inpAlertEmailEnabled = inpAlertEmailEnabled;
    _inpMinutesToWaitAfterPositionClosed = inpMinutesToWaitAfterPositionClosed;
    _inpMinTradingHour = inpMinTradingHour;
    _inpMaxTradingHour = inpMaxTradingHour;
    _inpMoveToBreakEven = inpMoveToBreakEven;

    if (!_fixedRisk.Init(&_symbol, PERIOD_CURRENT, 1)) {
        Print("Couldn't initialise fixed risk instance");
        return(INIT_FAILED);
    }

    _fixedRisk.Percent(inpDynamicSizingRiskPerTrade);

    _alreadyMovedToBreakEven = false;

    printf("DA=%f, adjusted points = %f", _digits_adjust, _adjustedPoints);

    ResetState();

    return(INIT_SUCCEEDED);
}

void CMyExpertBase::Deinit(void)
{
    Print("In base class OnDeInit");
    if (_atrHandle > 0) {
        Print("Releasing ATR indicator handle");
        ReleaseIndicator(_atrHandle);
    }
}

void CMyExpertBase::ResetState()
{
    _recentHigh = 0;
    _recentLow = 999999;
    _alreadyMovedToBreakEven = false;
    _previousOrderTotal = 0;

    _previousOrderTotal = 0;
    _eventCount = 0;
    _trailingStarted = false;
    _recentTurningPoint = 0;
    _hadRecentTurningPoint = false;
}

void CMyExpertBase::ReleaseIndicator(int& handle) {
    if (handle != INVALID_HANDLE && IndicatorRelease(handle)) {
        handle = INVALID_HANDLE;
    }
    else {
        Print("IndicatorRelease() failed. Error ", GetLastError());
    }
}

void CMyExpertBase::Processing(void)
{
    int takeProfitPipsFinal;
    double stopLossLevel;
    double takeProfitLevel;

    // -------------------- Collect most current data --------------------
    if (!RefreshRates()) {
        Print("Could not refresh rates during processing.");
        return;
    }

    int numberOfPriceDataPoints = CopyRates(_Symbol, 0, 0, 40, _prices);
    if (numberOfPriceDataPoints == -1) {
        Print("Error copying rates during processing.");
        return;
    }

    int atrDataCount = CopyBuffer(_atrHandle, 0, 0, 3, _atrData);
    if (atrDataCount == -1) {
        Print("Error copying ATR data.");
        return;
    }

    // -------------------- EXITS --------------------
    if (PositionSelect(_Symbol) == true) // We have an open position
    {
        CheckToModifyPositions();
    }

    //--- we work only at the time of the birth of new bar    
    if (!IsNewBar(iTime(0))) return;

    // -------------------- ENTRIES --------------------  
    if (PositionSelect(_Symbol) == false) // We have no open positions
    {        
        if (IsOutsideTradingHours()) {
            return;
        }

        if (ShouldPauseUntilOpeningNewPosition()) {
            return;
        }

        numberOfPriceDataPoints = CopyRates(_Symbol, 0, 0, 40, _prices);
        if (numberOfPriceDataPoints == -1) {
            Print("Error copying rates during processing.");
            return;
        }

        /*if (_inpTakeProfitPips < stopLevelPips) {
            takeProfitPipsFinal = stopLevelPips;
        }
        else {
            takeProfitPipsFinal = _inpTakeProfitPips;
        }*/
        takeProfitPipsFinal = _inpTakeProfitPips;

        double limitPrice;
        double lotSize = _inpLots;

        NewBarAndNoCurrentPositions();
        
        if (_inpGoLong && HasBullishSignal()) {
            limitPrice = _currentAsk;
            stopLossLevel = CalculateStopLossLevelForBuyOrder();

            if (_inpUseTakeProfit) {
                if (_inpTakeProfitRiskRewardRatio > 0) {
                    double distance = (limitPrice - stopLossLevel) * _inpTakeProfitRiskRewardRatio;
                    takeProfitLevel = limitPrice + distance;
                }
                else {
                    takeProfitLevel = limitPrice + takeProfitPipsFinal * _Point * _digits_adjust;
                }                
            }
            else {
                takeProfitLevel = 0.0;
            }

            if (_inpLotSizingRule == Dynamic) {
                lotSize = _fixedRisk.CheckOpenLong(limitPrice, stopLossLevel);
            }

            OpenPosition(_Symbol, ORDER_TYPE_BUY, lotSize, limitPrice, stopLossLevel, takeProfitLevel);
        }
        else if (_inpGoShort && HasBearishSignal()) {
            limitPrice = _currentBid;
            stopLossLevel = CalculateStopLossLevelForSellOrder();

            if (_inpUseTakeProfit) {
                if (_inpTakeProfitRiskRewardRatio > 0) {
                    double distance = (stopLossLevel - limitPrice) * _inpTakeProfitRiskRewardRatio;
                    takeProfitLevel = limitPrice - distance;
                }
                else {
                    takeProfitLevel = limitPrice - takeProfitPipsFinal * _Point * _digits_adjust;
                }                
            }
            else {
                takeProfitLevel = 0.0;
            }

            if (_inpLotSizingRule == Dynamic) {
                lotSize = _fixedRisk.CheckOpenShort(limitPrice, stopLossLevel);
            }

            OpenPosition(_Symbol, ORDER_TYPE_SELL, lotSize, limitPrice, stopLossLevel, takeProfitLevel);
        }
    }
}

void CMyExpertBase::OnTrade(void)
{
    _eventCount++;

    // The OnTrade event fires multiples times.  We're only interested in handling it on the third time.
    if (_eventCount < 3) return;
    _eventCount = 0;

    if (PositionSelect(_Symbol) == true) { // We have an open position
        _currentPositionType = _position.PositionType();
        return;
    }

    int minutes = 1;
    datetime to = TimeCurrent();
    datetime from = to - 60 * minutes;

    if (!HistorySelect(from, to)) {
        Print("Failed to retrieve order history");
    }

    int ordersTotal = HistoryOrdersTotal();
    if (ordersTotal <= 0) {
        Print("Orders total was 0");
        return;
    }

    // Select the last order to work with
    ulong ticket = HistoryOrderGetTicket(ordersTotal - 1);
    if (ticket == 0) {
        Print("Couldn't get last order ticket");
        return;
    }

    long orderState = HistoryOrderGetInteger(ticket, ORDER_STATE);
    if (orderState != ORDER_STATE_FILLED) {
        Print("Order state was not filled");
        return;
    }

    if (HistoryOrderGetString(ticket, ORDER_SYMBOL) != _symbol.Name()) {
        Print("Order was for a different pair");
        return;
    }

    long orderType = HistoryOrderGetInteger(ticket, ORDER_TYPE);

    /* Just in case we need to refer to other order types...
    case (ORDER_TYPE_BUY):            return("buy"); 
    case (ORDER_TYPE_SELL):           return("sell"); 
    case (ORDER_TYPE_BUY_LIMIT):      return("buy limit"); 
    case (ORDER_TYPE_SELL_LIMIT):     return("sell limit"); 
    case (ORDER_TYPE_BUY_STOP):       return("buy stop"); 
    case (ORDER_TYPE_SELL_STOP):      return("sell stop"); 
    case (ORDER_TYPE_BUY_STOP_LIMIT): return("buy stop limit"); 
    case (ORDER_TYPE_SELL_STOP_LIMIT):return("sell stop limit"); 
    */


    // We may refer to this

    //            switch (HistoryDealGetInteger(HistoryDealGetTicket(HistoryDealsTotal() - 1), DEAL_ENTRY))
    //            {
    //            case DEAL_ENTRY_IN:
    //                Print("We just entered the market.");
    //                break;
    //
    //            case DEAL_ENTRY_OUT:
    //                Print("We just exited our position.");
    //                break;
    //
    //            case DEAL_ENTRY_INOUT:
    //                Print("Close using inout");
    //                break;
    //
    //            case DEAL_ENTRY_OUT_BY:
    //                Print("We just closed our position by an opposite one.");
    //                break;
    //            }

    bool reset = false;
    if (_currentPositionType == POSITION_TYPE_BUY && orderType == ORDER_TYPE_SELL) {
        reset = true;
    }
    else if (_currentPositionType == POSITION_TYPE_SELL && orderType == ORDER_TYPE_BUY) {
        reset = true;
    }

    if (reset) {
        //Print("Resetting");
        _lastClosedPositionTime = TimeCurrent();
        ResetState();
        OnRecentlyClosedTrade();
    }
}

void CMyExpertBase::OnRecentlyClosedTrade()
{
}

void CMyExpertBase::NewBarAndNoCurrentPositions()
{
    Print("In base class NewBarAndNoCurrentPositions");
}

bool CMyExpertBase::ShouldPauseUntilOpeningNewPosition()
{
    if (_inpMinutesToWaitAfterPositionClosed == 0) {
        return false;
    }

    if (TimeCurrent() > _lastClosedPositionTime + 60 * _inpMinutesToWaitAfterPositionClosed) {
        return false;
    }

    return true;
}

////+------------------------------------------------------------------+ 
////| Get Time for specified bar index                                 | 
////+------------------------------------------------------------------+ 
datetime CMyExpertBase::iTime(const int index, string symbol = NULL, ENUM_TIMEFRAMES timeframe = PERIOD_CURRENT)
{
    if (symbol == NULL)
        symbol = _symbol.Name();
    if (timeframe == 0)
        timeframe = Period();
    datetime Time[1];
    datetime time = 0;
    int copied = CopyTime(symbol, timeframe, index, 1, Time);
    if (copied > 0) time = Time[0];
    return(time);
}

//+------------------------------------------------------------------+
//| Refreshes the symbol quotes data                                 |
//+------------------------------------------------------------------+
bool CMyExpertBase::RefreshRates()
{
    //--- refresh rates
    if (!_symbol.RefreshRates())
        return(false);

    //--- protection against the return value of "zero"
    if (_symbol.Ask() == 0 || _symbol.Bid() == 0)
        return(false);
    //---

    _currentBid = _symbol.Bid();
    _currentAsk = _symbol.Ask();

    return(true);
}

bool CMyExpertBase::CheckToModifyPositions()
{
    if (_inpTrailingStopLossRule == None && !_inpMoveToBreakEven) return false;

    if (!_position.Select(Symbol())) {
        return false;
    }

    if (_position.PositionType() == POSITION_TYPE_BUY) {
        if (LongModified())
            return true;
    }
    else {
        if (ShortModified())
            return true;
    }

    return false;
}

bool CMyExpertBase::LongModified()
{
    double newStop = 0;

    //if (_barsSincePositionOpened == 0) {
    //    return false;
    //}

    // Are we making higher highs?
    if (_prices[1].high > _prices[2].high && _prices[1].high > _recentHigh) {
        _recentHigh = _prices[1].high;
        _recentTurningPoint = _prices[1].low;
        _hadRecentTurningPoint = false;
    }
    else {        
        // Filter on _barsSincePositionOpened to give the position time to "breathe" (i.e. avoid moving SL too early after initial SL)
        if (_inpTrailingStopLossRule == ShortTermHighLow && !_hadRecentTurningPoint) {

            // For this SL rule we only operate after a new bar forms
            if (IsNewBar(iTime(0))) {
                _barsSincePositionOpened++;
                //Print("New bar found: ", _barsSincePositionOpened);
            }
            else {
                return false;
            }

            if (_barsSincePositionOpened < 3) return false;

            if (_prices[1].low < _recentTurningPoint && _prices[1].high < _recentHigh) {
                //Print("STH found: ", _recentHigh);

                // We have a short term high (STH).  Set SL to the low of the STH bar plus a margin
                newStop = _recentTurningPoint - _adjustedPoints * 6;
                _hadRecentTurningPoint = true;
            }
            else {
                // No new STH - nothing to do
                return false;
            }
        }
        else {
            return false;
        }
    }

    double breakEvenPoint = 0;
    //if (!_trailingStarted) {
    double initialRisk = _position.PriceOpen() - _initialStop;
    breakEvenPoint = _position.PriceOpen() + initialRisk;

    //    if (_currentAsk <= breakEvenPoint) {
    //        return false;
    //    }

    //    Print("Initiating trailing as we have hit breakeven");
    //    _trailingStarted = true;
    //}

    switch (_inpTrailingStopLossRule) {
        case StaticPipsValue:
            newStop = _recentHigh - _trailing_stop;
            break;

        case CurrentBar2Pips:
            newStop = _prices[1].low - _adjustedPoints * 2;
            break;

        case CurrentBar5Pips:
            newStop = _prices[1].low - _adjustedPoints * 5;
            break;

        case CurrentBar2ATR:
            newStop = _currentAsk - _atrData[0] * 2;
            break;

        case PreviousBar5Pips:
            // TODO: Check if previous bar high is actually higher!


            newStop = _prices[2].low - _adjustedPoints * 5;
            break;

        case PreviousBar2Pips:
            // TODO: Check if previous bar high is actually higher!
            newStop = _prices[2].low - _adjustedPoints * 2;
            break;
    }

    // TOOD: Is the new stop sufficiently far away or perhaps too far?


    // Check if we should move to breakeven
    //double risk;
    if (_inpMoveToBreakEven && !_alreadyMovedToBreakEven) {
        //risk = _position.PriceOpen() - _initialStop;
        //double breakEvenPoint = _position.PriceOpen() + risk;
        //    
        if (_currentAsk > breakEvenPoint) {
            if (newStop == 0.0 || breakEvenPoint > newStop) {
                printf("Moving to breakeven now that the price has reached %f", breakEvenPoint);
                newStop = _position.PriceOpen();
            }
        }
    }

    if (newStop == 0.0) {
        return false;
    }

    double sl = NormalizeDouble(newStop, _symbol.Digits());
    double tp = _position.TakeProfit();        
    double stopLevelPips = (double)(SymbolInfoInteger(_Symbol, SYMBOL_TRADE_STOPS_LEVEL) + SymbolInfoInteger(_Symbol, SYMBOL_SPREAD)) / _digits_adjust; // Defining minimum StopLevel

    if (_position.StopLoss() < sl || _position.StopLoss() == 0.0) {
        double diff = (_currentAsk - sl) / _adjustedPoints;
        if (diff < stopLevelPips) {
            printf("Can't set new stop that close to the current price.  Ask = %f, new stop = %f, stop level = %f, diff = %f",
                _currentAsk, sl, stopLevelPips, diff);

            sl = _currentAsk - stopLevelPips * _adjustedPoints;
        }

        //--- modify position
        if (!_trade.PositionModify(Symbol(), sl, tp)) {
            printf("Error modifying position for %s : '%s'", Symbol(), _trade.ResultComment());
            printf("Modify parameters : SL=%f,TP=%f", sl, tp);
        }

        if (!_alreadyMovedToBreakEven && sl >= _position.PriceOpen()) {
            int profitInPips = int((sl - _position.PriceOpen()) / _adjustedPoints);
            printf("%d pips profit now locked in (sl = %f, open = %f)", profitInPips, sl, _position.PriceOpen());
            _alreadyMovedToBreakEven = true;
        }            

        return true;
    }

    return false;
}

bool CMyExpertBase::ShortModified()
{
    //if (_barsSincePositionOpened == 0) {
    //    return false;
    //}

    double newStop = 0;

    if (!(_prices[1].low < _prices[2].low && _prices[1].low < _recentLow)) {
        return false;
    }

    _recentLow = _prices[1].low;

    double breakEvenPoint = 0;
    //if (!_trailingStarted) {
    //    double risk = _initialStop - _position.PriceOpen();
    //    breakEvenPoint = _position.PriceOpen() - risk;

    //    if (_currentAsk > breakEvenPoint) {
    //        return false;
    //    }

    //    Print("Initiating trailing as we have hit breakeven");
    //    _trailingStarted = true;
    //}

    switch (_inpTrailingStopLossRule) {
        case StaticPipsValue:
            newStop = _recentLow + _trailing_stop;
            break;

        case CurrentBar2Pips:
            newStop = _prices[1].high + _adjustedPoints * 2;
            break;

        case CurrentBar5Pips:
            newStop = _prices[1].high + _adjustedPoints * 5;
            break;

        case CurrentBar2ATR:
            newStop = _currentBid + _atrData[0] * 2;
            break;

        case PreviousBar5Pips:
            newStop = _prices[2].high + _adjustedPoints * 5;
            break;

        case PreviousBar2Pips:
            newStop = _prices[2].high + _adjustedPoints * 2;
            break;
    }

    // Check if we should move to breakeven        
    if (_inpMoveToBreakEven && !_alreadyMovedToBreakEven) {
        //double risk = _initialStop - _position.PriceOpen();
        //double breakEvenPoint = _position.PriceOpen() - risk;
        if (_currentAsk < breakEvenPoint) {
            if (newStop == 0.0 || breakEvenPoint < newStop) {
                printf("Moving to breakeven now that the price has reached %f", breakEvenPoint);
                newStop = _position.PriceOpen();
            }
        }
    }

    if (newStop == 0.0) {
        return false;
    }

    double sl = NormalizeDouble(newStop, _symbol.Digits());
    double stopLevelPips = (double)(SymbolInfoInteger(_Symbol, SYMBOL_TRADE_STOPS_LEVEL) + SymbolInfoInteger(_Symbol, SYMBOL_SPREAD)) / _digits_adjust; // Defining minimum StopLevel
    double tp = _position.TakeProfit();

    if (_position.StopLoss() > sl || _position.StopLoss() == 0.0) {

        double diff = (sl - _currentBid) / _adjustedPoints;
        if (diff < stopLevelPips) {
            printf("Can't set new stop that close to the current price.  Bid = %f, new stop = %f, stop level = %f, diff = %f",
                _currentBid, sl, stopLevelPips, diff);

            sl = _currentBid + stopLevelPips * _adjustedPoints;
        }

        //--- modify position
        if (!_trade.PositionModify(Symbol(), sl, tp)) {
            printf("Error modifying position for %s : '%s'", Symbol(), _trade.ResultComment());
            printf("Modify parameters : SL=%f,TP=%f", sl, tp);
        }

        if (!_alreadyMovedToBreakEven && sl <= _position.PriceOpen()) {
            int profitInPips = int((_position.PriceOpen() - sl) / _adjustedPoints);
            printf("%d pips profit now locked in (sl = %f, open = %f)", profitInPips, sl, _position.PriceOpen());
            _alreadyMovedToBreakEven = true;
        }

        return true;
    }

    return false;
}

bool CMyExpertBase::HasBullishSignal()
{
    return false;
}

bool CMyExpertBase::HasBearishSignal()
{
    return false;
}

void CMyExpertBase::OpenPosition(string symbol, ENUM_ORDER_TYPE orderType, double volume, double price, double stopLoss, double takeProfit)
{
    string message;
    string orderTypeMsg;

    switch (orderType) {
        case ORDER_TYPE_BUY:
            orderTypeMsg = "Buy";
            message = "Going long. Magic Number #" + (string)_trade.RequestMagic();
            break;

        case ORDER_TYPE_SELL:
            orderTypeMsg = "Sell";
            message = "Going short. Magic Number #" + (string)_trade.RequestMagic();
            break;

        case ORDER_TYPE_BUY_LIMIT:
            orderTypeMsg = "Buy limit";
            message = "Going long at " + (string)price + ". Magic Number #" + (string)_trade.RequestMagic();
            break;

        case ORDER_TYPE_SELL_LIMIT:
            orderTypeMsg = "Sell limit";
            message = "Going short at " + (string)price + ". Magic Number #" + (string)_trade.RequestMagic();
            break;
    }

    if (_inpAlertTerminalEnabled) {
        Alert(message);
    }

    if (_trade.PositionOpen(symbol, orderType, volume, price, stopLoss, takeProfit, message)) {
        _initialStop = 0;
        uint resultCode = _trade.ResultRetcode();
        if (resultCode == TRADE_RETCODE_PLACED || resultCode == TRADE_RETCODE_DONE) {
            Print("Entry rules: A ", orderTypeMsg, " order has been successfully placed with Ticket#: ", _trade.ResultOrder());
            _barsSincePositionOpened = 0;
            _initialStop = stopLoss;
        }
        else {
            Print("Entry rules: The ", orderTypeMsg, " order request could not be completed.  Result code: ", resultCode, ", Error: ", GetLastError());
            ResetLastError();
            return;
        }
    }
}

bool CMyExpertBase::IsOutsideTradingHours()
{
    MqlDateTime currentTime;
    TimeToStruct(TimeCurrent(), currentTime);
    if (_inpMinTradingHour > 0 && currentTime.hour < _inpMinTradingHour) {
        return true;
    }

    if (_inpMaxTradingHour > 0 && currentTime.hour > _inpMaxTradingHour) {
        return true;
    }

    return false;
}

double CMyExpertBase::CalculateStopLossLevelForBuyOrder()
{
    double stopLossPipsFinal;
    double stopLossLevel = 0;
    double stopLevelPips;
    double low;
    double priceFromStop;
    double pips = 5;

    stopLevelPips = (double)(SymbolInfoInteger(_Symbol, SYMBOL_TRADE_STOPS_LEVEL) + SymbolInfoInteger(_Symbol, SYMBOL_SPREAD)) / _digits_adjust; // Defining minimum StopLevel
    switch (_inpInitialStopLossRule) {
        case None:
            stopLossLevel = 0;
            break;

        case StaticPipsValue:           
            if (_inpInitialStopLossPips < stopLevelPips) {
                stopLossPipsFinal = stopLevelPips;
            }
            else {
                stopLossPipsFinal = _inpInitialStopLossPips;
            }

            stopLossLevel = _currentAsk - stopLossPipsFinal * _Point * _digits_adjust;
            break;

        case CurrentBar2Pips:
            // Fall-through
        case CurrentBarNPips:
            // Fall-through
        case CurrentBar5Pips:            
            if (_inpInitialStopLossRule == CurrentBar2Pips) {
                pips = 2;
            }
            else if (_inpInitialStopLossRule == CurrentBarNPips) {
                pips = _inpInitialStopLossPips;
            }

            stopLossLevel = _prices[1].low - _adjustedPoints * pips;
            priceFromStop = (_currentAsk - stopLossLevel) / (_Point * _digits_adjust);

            if (priceFromStop < stopLevelPips) {
                printf("calculated stop too close to price.  adjusting from %f to %f", priceFromStop, stopLevelPips);
                stopLossPipsFinal = stopLevelPips;
            }
            else {
                stopLossPipsFinal = priceFromStop;
            }

            stopLossLevel = _currentAsk - stopLossPipsFinal * _Point * _digits_adjust;
            break;

        case CurrentBar2ATR:
            stopLossLevel = _currentAsk - _atrData[0] * 2;
            break;

        case PreviousBar5Pips:
            low = _prices[1].low;
            if (_prices[2].low < low) {
                low = _prices[2].low;
            }

            stopLossLevel = low - _adjustedPoints * 5;
            break;

        case PreviousBar2Pips:
            low = _prices[1].low;
            if (_prices[2].low < low) {
                low = _prices[2].low;
            }

            stopLossLevel = low - _adjustedPoints * 2;
            break;
    }

    double sl = NormalizeDouble(stopLossLevel, _symbol.Digits());
    return sl;
}

double CMyExpertBase::CalculateStopLossLevelForSellOrder()
{
    double stopLossPipsFinal;
    double stopLossLevel = 0;
    double stopLevelPips;
    double high;
    double pips = 5;

    stopLevelPips = (double)(SymbolInfoInteger(_Symbol, SYMBOL_TRADE_STOPS_LEVEL) + SymbolInfoInteger(_Symbol, SYMBOL_SPREAD)) / _digits_adjust; // Defining minimum StopLevel
    switch (_inpInitialStopLossRule) {
        case None:
            stopLossLevel = 0;
            break;

        case StaticPipsValue:
            if (_inpInitialStopLossPips < stopLevelPips) {
                stopLossPipsFinal = stopLevelPips;
            }
            else {
                stopLossPipsFinal = _inpInitialStopLossPips;
            }

            stopLossLevel = _currentBid + stopLossPipsFinal * _Point * _digits_adjust;
            break;

        case CurrentBar2Pips:
            // Fall-through
        case CurrentBarNPips:
            // Fall-through
        case CurrentBar5Pips:            
            if (_inpInitialStopLossRule == CurrentBar2Pips) {
                pips = 2;
            }
            else if (_inpInitialStopLossRule == CurrentBarNPips) {
                pips = _inpInitialStopLossPips;
            }

            stopLossLevel = _prices[1].high + _adjustedPoints * pips;
            break;

        case CurrentBar2ATR:
            stopLossLevel = _currentBid + _atrData[0] * 2;
            break;

        case PreviousBar5Pips:
            high = _prices[1].high;
            if (_prices[2].high > high) {
                high = _prices[2].high;
            }

            stopLossLevel = high + _adjustedPoints * 5;
            break;

        case PreviousBar2Pips:
            high = _prices[1].high;
            if (_prices[2].high > high) {
                high = _prices[2].high;
            }

            stopLossLevel = high + _adjustedPoints * 2;
            break;
    }

    double priceFromStop = (stopLossLevel - _currentBid) / (_Point * _digits_adjust);

    Print("Price from stop: ", priceFromStop);
    if (priceFromStop < stopLevelPips) {
        printf("calculated stop too close to price.  adjusting from %f to %f", priceFromStop, stopLevelPips);
        stopLossPipsFinal = stopLevelPips;
    }
    else {
        stopLossPipsFinal = priceFromStop;
    }

    stopLossLevel = _currentBid + stopLossPipsFinal * _Point * _digits_adjust;

    return NormalizeDouble(stopLossLevel, _symbol.Digits());
}

bool CMyExpertBase::IsNewBar(datetime currentTime)
{
    bool result = false;
    if (_barTime != currentTime)
    {
        _barTime = currentTime;
        result = true;
    }

    return result;
}