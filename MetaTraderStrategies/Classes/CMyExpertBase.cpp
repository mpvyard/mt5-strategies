#include <Trade\Trade.mqh>
#include <Trade\SymbolInfo.mqh> 
#include <Trade\PositionInfo.mqh>

enum STOPLOSS_RULE
{
    StaticPipsValue,
    CurrentBar5Pips,
    CurrentBar2ATR,
    PreviousBar5Pips
};

class CMyExpertBase
{
public:
    CMyExpertBase(void);
    ~CMyExpertBase(void);

    virtual int Init
    (
        double          inpLots,
        STOPLOSS_RULE   inpInitialStopLossRule,
        double          inpInitialStopLossPips,
        bool            inpUseTakeProfit,
        double          inpTakeProfitPips,
        STOPLOSS_RULE   inpTrailingStopLossRule,
        int             inpTrailingStopPips,
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
    
    double _inpLots;
    STOPLOSS_RULE _inpInitialStopLossRule;
    double _inpInitialStopLossPips;
    bool _inpUseTakeProfit;
    double _inpTakeProfitPips;
    STOPLOSS_RULE _inpTrailingStopLossRule;
    bool _inpGoLong;
    bool _inpGoShort;
    bool _inpAlertTerminalEnabled;
    bool _inpAlertEmailEnabled;
    int _inpMinutesToWaitAfterPositionClosed;
    int _inpMinTradingHour;
    int _inpMaxTradingHour;

    void ReleaseIndicator(int& handle);
    virtual void NewBarAndNoCurrentPositions();

private:
    bool RefreshRates();
    datetime iTime(const int index, string symbol = NULL, ENUM_TIMEFRAMES timeframe = PERIOD_CURRENT);
    bool CheckToModifyPositions();
    void OpenPosition(string symbol, ENUM_ORDER_TYPE orderType, double volume, double price, double stopLoss, double takeProfit);
    bool LongModified();
    bool ShortModified();
    bool IsOutsideTradingHours();
    double CalculateStopLossLevelForBuyOrder();
    double CalculateStopLossLevelForSellOrder();
    bool RecentlyClosedTrade();

    double _recentHigh;
    double _recentLow;
    double _atrData[];
    int _atrHandle;
};

CMyExpertBase::CMyExpertBase(void)
{
}

CMyExpertBase::~CMyExpertBase(void)
{
}

int CMyExpertBase::Init(
    double          inpLots,
    STOPLOSS_RULE   inpInitialStopLossRule,
    double          inpInitialStopLossPips,
    bool            inpUseTakeProfit,
    double          inpTakeProfitPips,
    STOPLOSS_RULE   inpTrailingStopLossRule,
    int             inpTrailingStopPips,
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

    ArraySetAsSeries(_prices, true);
    ArraySetAsSeries(_atrData, true);
    //_maHandle = iMA(Symbol(), _inpMovingAveragePeriodType, _inpMovingAveragePeriodAmount, 0, MODE_SMA, PRICE_CLOSE);
    _atrHandle = iATR(_Symbol, 0, 14);

    _digits_adjust = 1;
    if (_Digits == 5 || _Digits == 3 || _Digits == 1) {
        _digits_adjust = 10;
    }

    _adjustedPoints = _symbol.Point() * _digits_adjust;

    _inpLots = inpLots;
    _inpInitialStopLossRule = inpInitialStopLossRule;
    _inpInitialStopLossPips = inpInitialStopLossPips;
    _inpUseTakeProfit = inpUseTakeProfit;
    _inpTakeProfitPips = inpTakeProfitPips;
    _inpTrailingStopLossRule = inpTrailingStopLossRule;
    _inpGoLong = inpGoLong;
    _inpGoShort = inpGoShort;

    _trailing_stop = inpTrailingStopPips * _adjustedPoints;

    _inpAlertTerminalEnabled = inpAlertTerminalEnabled;
    _inpAlertEmailEnabled = inpAlertEmailEnabled;
    _inpMinutesToWaitAfterPositionClosed = inpMinutesToWaitAfterPositionClosed;
    _inpMinTradingHour = inpMinTradingHour;
    _inpMaxTradingHour = inpMaxTradingHour;

    printf("DA=%f, adjusted points = %f", _digits_adjust, _adjustedPoints);

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
    //--- we work only at the time of the birth of new bar
    static datetime PrevBars = 0;

    datetime time_0 = iTime(0);
    if (time_0 == PrevBars) return;

    PrevBars = time_0;

    double takeProfitPipsFinal;
    double stopLossLevel;
    double takeProfitLevel;

    // -------------------- Collect most current data --------------------
    if (!RefreshRates()) {
        return;
    }

    int numberOfPriceDataPoints = CopyRates(_Symbol, 0, 0, 40, _prices);
    int atrDataCount = CopyBuffer(_atrHandle, 0, 0, 3, _atrData);

    // -------------------- EXITS --------------------

    if (PositionSelect(_Symbol) == true) // We have an open position
    {
        CheckToModifyPositions();
        return;
    }

    // -------------------- ENTRIES --------------------  
    if (PositionSelect(_Symbol) == false) // We have no open positions
    {
        if (IsOutsideTradingHours()) {
            return;
        }

        if (RecentlyClosedTrade()) {
            return;
        }

        _recentHigh = 0;
        _recentLow = 999999;

        numberOfPriceDataPoints = CopyRates(_Symbol, 0, 0, 40, _prices);

        /*if (_inpTakeProfitPips < stopLevelPips) {
            takeProfitPipsFinal = stopLevelPips;
        }
        else {
            takeProfitPipsFinal = _inpTakeProfitPips;
        }*/
        takeProfitPipsFinal = _inpTakeProfitPips;

        double limitPrice;

        NewBarAndNoCurrentPositions();

        if (_inpGoLong && HasBullishSignal()) {
            limitPrice = _currentAsk;
            stopLossLevel = CalculateStopLossLevelForBuyOrder();

            if (_inpUseTakeProfit) {
                takeProfitLevel = limitPrice + takeProfitPipsFinal * _Point * _digits_adjust;
            }
            else {
                takeProfitLevel = 0.0;
            }

            OpenPosition(_Symbol, ORDER_TYPE_BUY, _inpLots, limitPrice, stopLossLevel, takeProfitLevel);
        }
        else if (_inpGoShort && HasBearishSignal()) {
            limitPrice = _currentBid;
            stopLossLevel = CalculateStopLossLevelForSellOrder();

            if (_inpUseTakeProfit) {
                takeProfitLevel = limitPrice - takeProfitPipsFinal * _Point * _digits_adjust;
            }
            else {
                takeProfitLevel = 0.0;
            }

            OpenPosition(_Symbol, ORDER_TYPE_SELL, _inpLots, limitPrice, stopLossLevel, takeProfitLevel);
        }
    }
}

void CMyExpertBase::NewBarAndNoCurrentPositions()
{
    Print("In base class NewBarAndNoCurrentPositions");
}

bool CMyExpertBase::RecentlyClosedTrade()
{
    datetime to = TimeCurrent();
    datetime from = to - 60 * _inpMinutesToWaitAfterPositionClosed;

    if (!HistorySelect(from, to)) {
        Print("Failed to retrieve order history");
        return false;
    }

    uint orderCount = HistoryOrdersTotal();
    if (orderCount <= 0) return false;

    ulong ticket;
    //--- return order ticket by its position in the list 
    if ((ticket = HistoryOrderGetTicket(orderCount - 1)) > 0) {
        if (HistoryOrderGetString(ticket, ORDER_SYMBOL) == _symbol.Name()) {
            if (HistoryOrderGetInteger(ticket, ORDER_TYPE) == ORDER_TYPE_SELL) {
                // Print("We had a recent sell order so we'll wait a bit");
                return true;
            }
        }
    }

    return false;
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
    if (_inpTrailingStopLossRule == StaticPipsValue && _trailing_stop == 0) return false;

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

    if (_prices[1].high > _prices[2].high && _prices[1].high > _recentHigh) {
        _recentHigh = _prices[1].high;

        switch (_inpTrailingStopLossRule) {
            case StaticPipsValue:
                if (_trailing_stop <= 0) return false;
                newStop = _recentHigh - _trailing_stop;
                break;

            case CurrentBar5Pips:
                newStop = _prices[1].low - _symbol.Point() * 5;
                break;

            case CurrentBar2ATR:
                newStop = _currentAsk - _atrData[0] * 2;
                break;

            case PreviousBar5Pips:
                newStop = _prices[2].low - _symbol.Point() * 5;
                break;
        }

        double sl = NormalizeDouble(newStop, _symbol.Digits());
        double tp = _position.TakeProfit();
        if (_position.StopLoss() < sl || _position.StopLoss() == 0.0) {
            //--- modify position
            if (!_trade.PositionModify(Symbol(), sl, tp)) {
                printf("Error modifying position by %s : '%s'", Symbol(), _trade.ResultComment());
                printf("Modify parameters : SL=%f,TP=%f", sl, tp);
            }

            return true;
        }
    }

    return false;
}

bool CMyExpertBase::ShortModified()
{
    double newStop = 0;

    if (_prices[1].low < _prices[2].low && _prices[1].low < _recentLow) {
        printf("A new low found (%f). Prev low: %f and recent low: %f", _prices[1].low, _prices[2].low, _recentLow);
        _recentLow = _prices[1].low;

        switch (_inpTrailingStopLossRule) {
            case StaticPipsValue:
                if (_trailing_stop <= 0) return false;
                newStop = _recentLow + _trailing_stop;
                break;

            case CurrentBar5Pips:
                newStop = _prices[1].high +_symbol.Point() * 5;
                break;

            case CurrentBar2ATR:
                newStop = _currentBid + _atrData[0] * 2;
                break;

            case PreviousBar5Pips:
                newStop = _prices[2].high + _symbol.Point() * 5;
                break;
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
                printf("Error modifying position by %s : '%s'", Symbol(), _trade.ResultComment());
                printf("Modify parameters : SL=%f,TP=%f", sl, tp);
            }

            return true;
        }
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
        uint resultCode = _trade.ResultRetcode();
        if (resultCode == TRADE_RETCODE_PLACED || resultCode == TRADE_RETCODE_DONE) {
            Print("Entry rules: A ", orderTypeMsg, " order has been successfully placed with Ticket#: ", _trade.ResultOrder());
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

    switch (_inpInitialStopLossRule) {
        case StaticPipsValue:
            stopLevelPips = (double)(SymbolInfoInteger(_Symbol, SYMBOL_TRADE_STOPS_LEVEL) + SymbolInfoInteger(_Symbol, SYMBOL_SPREAD)) / _digits_adjust; // Defining minimum StopLevel
            if (_inpInitialStopLossPips < stopLevelPips) {
                stopLossPipsFinal = stopLevelPips;
            }
            else {
                stopLossPipsFinal = _inpInitialStopLossPips;
            }

            stopLossLevel = _currentAsk - stopLossPipsFinal * _Point * _digits_adjust;
            break;

        case CurrentBar5Pips:
            stopLossLevel = _prices[1].low - _symbol.Point() * 5;
            break;

        case CurrentBar2ATR:
            stopLossLevel = _currentAsk - _atrData[0] * 2;
            break;

        case PreviousBar5Pips:
            low = _prices[1].low;
            if (_prices[2].low < low) {
                low = _prices[2].low;
            }

            stopLossLevel = low - _symbol.Point() * 5;
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

    switch (_inpInitialStopLossRule) {
        case StaticPipsValue:
            stopLevelPips = (double)(SymbolInfoInteger(_Symbol, SYMBOL_TRADE_STOPS_LEVEL) + SymbolInfoInteger(_Symbol, SYMBOL_SPREAD)) / _digits_adjust; // Defining minimum StopLevel

            if (_inpInitialStopLossPips < stopLevelPips) {
                stopLossPipsFinal = stopLevelPips;
            }
            else {
                stopLossPipsFinal = _inpInitialStopLossPips;
            }

            stopLossLevel = _currentBid + stopLossPipsFinal * _Point * _digits_adjust;
            break;

        case CurrentBar5Pips:
            stopLossLevel = _prices[1].high + _symbol.Point() * 5;
            break;

        case CurrentBar2ATR:
            stopLossLevel = _currentBid + _atrData[0] * 2;
            break;

        case PreviousBar5Pips:
            high = _prices[1].high;
            if (_prices[2].high > high) {
                high = _prices[2].high;
            }

            stopLossLevel = high + _symbol.Point() * 5;
            break;
    }

    double sl = NormalizeDouble(stopLossLevel, _symbol.Digits());
    return sl;
}