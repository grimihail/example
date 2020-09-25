/*
    \file Payment.cpp

    \author Mihailovskii G.
    
    \date 2020
*/

#include "cicPayment.h"

#ifdef _PAYMENT_

#include "cicClock.h"
#include "ExportFs.h"
#include "cicDC.h"
#include "stdlib.h"
#include "utils.h"
#include "apduTask.h"
#include "acse.h"
#include "_objectMaps.h"
#include "cicEventAlarmErrorState.h"
#include "wildcards.h"
#include "alarms.h"
#include "States.h"

/************************************************************************************************/
/*************************Default configurations for Payment objects****************************/
/************************************************************************************************/
/* Import objects */
/* Default (whenever a meter is newly installed) configuration of Import Account */
PaymentAccountCfg defaultImportAccountCfg = {
    .modeAndStatus              = { prepaymentMode, newAccount },       // 2 (creditMode may not change by consumer)
    .clearanceThreshold         = 0,                                    // 7 (may not change by consumer)
    .creditRefList              = { &PaymentImportCreditLn },           // 9 (may not change by consumer)
    .chargeRefList              = { &PaymentActiveImportChargeLn },     // 10 (may not change by consumer)
    .creditChargeCfg            = { {&PaymentImportCreditLn, &PaymentActiveImportChargeLn, 0} },        // 11 (may not change by consumer)
    .tokenGatewayCfg            = { /*0*/ {PaymentImportCreditLn, 100} },     // 12 (may not change by consumer)
    .accountActivationTime      = {0},                                  // 13 (may change by consumer or by soft)
    .accountClosureTime         = {0},                                  // 14 (may change by consumer or by soft)
    .currency                   = { { 'M', 'D', 'L' }, -2, currencyUnitMonetary },                      // 15 (may not change by consumer)
    .maxProvision               = 0,                                    // 18 (may be set by consumer)
    .maxProvisionPeriod         = 0                                     // 19 (may be set by consumer)
};

/* Default (whenever a meter is newly installed) configuration of Import Credit */
PaymentCreditCfg defaultImportCreditCfg = {
    .creditType                 = tokenCredit,  // 3 (may not change by consumer)
    .priority                   = 1,            // 4 (may not change by consumer)
    .warningThreshold           = 0,            // 5 (may be set by consumer or may remain 0)
    .limit                      = 0,            // 6 (may be set by consumer or may remain 0)
    .creditConfiguration        = 0,            // 7 (may not change by consumer)
    .presetCreditAmount         = 0,            // 9 (may not change by consumer)
    .creditAvailableThreshold   = 0,            // 10 (may not change by consumer)
    .period                     = {0}           // 11 (may not change by consumer) (if credit_type = time_based_credit/consumption_based_credit then must be set by consumer)    
};

/* Default (whenever a meter is newly installed) configuration of Active TOU Import Charge */
PaymentChargeCfg defaultActiveImportChargeCfg = {
    .chargeType                 = PaymentChargeConsumptionBased,        // 3 (may not change by consumer)
    .priority                   = 1,                                    // 4 (may not change by consumer)
    .unitChargeActive           = {0},                                  // 5 (may be changed by soft)
    .unitChargePassive          = { {-3, -2}, {Register, RegisterAsumLN, 3}, {{{0}, 184}} }, // 6 (may be set by consumer)      { {1, 2}, {Register, &RegisterAsumLN, 3}, {{{4, 5}, 6}, {{7, 8}, 9}} }
    .unitChargeActivationTime   = {0},                                  // 7 (may be set by consumer)
    .period                     = 60,                                   // 8 (may be set by consumer)
    .chargeConfiguration        = 0 | chargeContinuousCollection,       // 9 (may not change by consumer)
    .proportion                 = 0                                     // 13 (may not change by consumer)    
};

ftPaymentAccount ftImportAccount =
{
    .ftAccountStatus            =       ftImportAccount_AccountStatus,
    .ftAccountActivationTime    =       ftImportAccount_AccountActivationTime,
    .ftAccountClosureTime       =       ftImportAccount_AccountClosureTime,
    .ftMaxProvision             =       ftImportAccount_MaxProvision,
    .ftMaxProvisionPeriod       =       ftImportAccount_MaxProvisionPeriod,
    .ftCurrency                 =       ftImportAccount_Currency
};

ftPaymentCredit ftImportCredit =
{
    .ftCurrentCreditAmount      =       ftImportCredit_CurrentCreditAmountQ,
    .ftWarningThreshold         =       ftImportCredit_WarningThreshold,
    .ftLimit                    =       ftImportCredit_Limit,
    .ftCreditStatus             =       ftImportCredit_CreditStatus
};

ftPaymentCharge ftActiveImportCharge =
{
    .ftTotalAmountPaid          =       ftActiveImportCharge_TotalAmountPaidQ,
    .ftUnitChargeActive         =       ftActiveImportCharge_UnitChargeActive,
    .ftUnitChargePassive        =       ftActiveImportCharge_UnitChargePassive,
    .ftUnitChargeActivationTime =       ftActiveImportCharge_UnitChargeActivationTime,
    .ftPeriod                   =       ftActiveImportCharge_Period,
    .ftLastCollectionTime       =       ftActiveImportCharge_LastCollectionTimeQ,
    .ftLastCollectionAmount     =       ftActiveImportCharge_LastCollectionAmountQ,
    .ftTotalAmountRemaining     =       ftActiveImportCharge_TotalAmountRemainingQ,
    .ftLastMeasurementValue     =       ftActiveImportCharge_LastMeasurementValueQ,
    .ftSumToCollect             =       ftActiveImportCharge_SumToCollectQ
};

ftPaymentTokenGateway ftTokenGatewayForImportAccount =
{
    .ftToken                    =       ftImportTokenGateway_Token,
    .ftTokenTime                =       ftImportTokenGateway_TokenTime,
    .ftTokenDeliveryMethod      =       ftImportTokenGateway_TokenDeliveryMethod,
    .ftTokenStatusCode          =       ftImportTokenGateway_TokenStatusCode,
    .ftTokenID                  =       ftImportTokenGateway_TokenID,
    .ftNextReceivedTokenIndex   =       ftImportTokenGateway_NextReceivedTokenIndex,
    .ftActiveTransactionID      =       ftImportTokenGateway_ActiveTransactionID,
    .ftLastTokenSubtype         =       ftImportTokenGateway_LastTokenSubtype,
    .ftExpiresTimeSecReceivedWithStart  = ftImportTokenGateway_ExpiresTime,
    .ftExpiresTimeStatusReceivedWithStart = ftImportTokenGateway_ExpiresTimeStatus,
    .ftTimeOfStartSec           =       ftImportTokenGateway_TimeOfStart,
    .ftTimeOfStartStatus        =       ftImportTokenGateway_TimeOfStartStatus
};

//ftStartStopServiceFile ftStartStopService =
//{
//    .ftActiveTransactionID      =       ftStartStopRRMService_ActiveTransactionID
//};

/************************************************************************************************/
/*************************Initialization of Payment objects**************************************/
/************************************************************************************************/

#ifdef NEW_CONST_CLASS_MAP
CIC_STS_CREDIT_ADD_OBJECT( PaymentImportCreditLn, PaymentCreditClass, PaymentImportCredit, (&PaymentImportCreditLn, &defaultImportCreditCfg, &ftImportCredit) );
CIC_STS_CHARGE_ADD_OBJECT( PaymentActiveTouImportChargeLn, PaymentChargeClass, PaymentActiveTouImportCharge, (&PaymentActiveTouImportChargeLn, &defaultActiveTouImportChargeCfg, &ftActiveImportCharge) );

CIC_STS_TOKEN_GATEWAY_ADD_OBJECT( PaymentImportTokenGatewayLn, PaymentTokenGatewayClass, TokenGatewayForImportAccount, (&PaymentImportTokenGatewayLn, &defaultTokenGatewayCfg, &ftTokenGatewayForImportAccount) );
#else
PaymentCreditClass PaymentImportCredit( &PaymentImportCreditLn, &defaultImportCreditCfg, &ftImportCredit );
PaymentChargeClass PaymentActiveImportCharge( &PaymentActiveImportChargeLn, &defaultActiveImportChargeCfg, &ftActiveImportCharge );

PaymentTokenGatewayClass TokenGatewayForImportAccount( &PaymentImportTokenGatewayLn, &ftTokenGatewayForImportAccount );
#endif

PaymentCreditClass* importAccountCreditList[MAX_OBJECTS_IN_CREDIT_REF_LIST] =
{
    &PaymentImportCredit
};

PaymentChargeClass* importAccountChargeList[MAX_OBJECTS_IN_CHARGE_REF_LIST] =
{
    &PaymentActiveImportCharge
};

#ifdef NEW_CONST_CLASS_MAP
CIC_STS_ACCOUNT_ADD_OBJECT( PaymentImportAccountLn, PaymentAccountClass, PaymentImportAccount, (&PaymentImportAccountLn, &defaultImportAccountCfg, importAccountCreditList, importAccountChargeList, &TokenGatewayForImportAccount, &ftImportAccount) );
#else
PaymentAccountClass PaymentImportAccount( &PaymentImportAccountLn, &defaultImportAccountCfg, importAccountCreditList, importAccountChargeList, &TokenGatewayForImportAccount, &ftImportAccount );
#endif

//#ifdef NEW_CONST_CLASS_MAP
//CIC_DATA_ADD_OBJECT( StartStopServiceLn, StartStopServiceClass, StartStopService, (&StartStopServiceLn, &PaymentImportAccountClass, &ftStartStopService) );
//#else
//StartStopServiceClass StartStopService( &StartStopServiceLn, &PaymentImportAccountClass, &ftStartStopService );
//#endif

#ifdef NEW_CONST_CLASS_MAP
CIC_DATA_ADD_OBJECT( OutTokenLn, OutTokenClass, OutTokenObject, (&OutTokenLn) );
CIC_DATA_ADD_OBJECT( ActiveTransactionIDLn, ActiveTransactionIDClass, ActiveTransactionIDObject, (&ActiveTransactionIDLn) );
CIC_REGISTER_ADD_OBJECT( TopUpsSumLn, TopUpsSumClass, TopUpsSumObject, (&TopUpsSumLn) );
CIC_REGISTER_ADD_OBJECT( TotalAmountPaidLn, TotalAmountPaidClass, TotalAmountPaidObject, (&TotalAmountPaidLn) );
CIC_REGISTER_ADD_OBJECT( ConsumedKWhFromStartLn, ConsumedKWhFromStartClass, ConsumedKWhFromStartObject, (&ConsumedKWhFromStartLn) );
CIC_DATA_ADD_OBJECT( TokenIDLn, TokenIDClass, TokenIDObject, (&TokenIDLn) );
CIC_DATA_ADD_OBJECT( ExpiresTimeLn, ExpiresTimeClass, ExpiresTimeObject, (&ExpiresTimeLn) );
#else
OutTokenClass OutTokenObject( &OutTokenLn );
ActiveTransactionIDClass ActiveTransactionIDObject( &ActiveTransactionIDLn );
TopUpsSumClass TopUpsSumObject( &TopUpsSumLn );
TotalAmountPaidClass TotalAmountPaidObject( &TotalAmountPaidLn );
ConsumedKWhFromStartClass ConsumedKWhFromStartObject( &ConsumedKWhFromStartLn );
TokenIDClass TokenIDObject( &TokenIDLn );
ExpiresTimeClass ExpiresTimeObject ( &ExpiresTimeLn );
#endif

/************************************************************************************************/
/*********************************** Assist Functions********************************************/
/************************************************************************************************/

u32 getUTCSecondsWithCorrection( TDateTime dateTime )
{
#warning: "10 years correction because in cicClock UTS seconds calculates from 1980 year"
    dateTime.date.year_low += 10;
    return GetSecondsUTC( &dateTime );
}

u32 getCurrentUTCSecondsWithCorrection()
{
    TDateTime currDateTime;
    GetLocalTime_APDU( &currDateTime );
    return getUTCSecondsWithCorrection( currDateTime );
}

void SecondsUTC_To_Local_DateTime_WithCorrection( DWORD seconds, TDateTime* dateTime )
{
#warning: "10 years correction because in cicClock UTS seconds calculates from 1980 year"
    SecondsTo_Local_DateTime( seconds, dateTime );
    dateTime->date.year_low -= 10;
    
    u16 year = dateTime->date.year_hi << 8 | dateTime->date.year_low;
    dateTime->date.w_day = GetWeekDayFromDate( year, dateTime->date.month, dateTime->date.day );
}

bool cmpDateTimeStructs( const TDateTime* const DateTime1, const TDateTime* const DateTime2 )
{
    if( DateTime1->date.year_hi == DateTime2->date.year_hi &&
        DateTime1->date.year_low == DateTime2->date.year_low &&
        DateTime1->date.month == DateTime2->date.month &&
        DateTime1->date.day == DateTime2->date.day &&
        DateTime1->date.w_day == DateTime2->date.w_day &&
        DateTime1->time.hour == DateTime2->time.hour &&
        DateTime1->time.minute == DateTime2->time.minute &&
        DateTime1->time.second == DateTime2->time.second )
          return true;
    return false;
}

bool cmpDateTimeWithCurrDateTime( const TDateTime* const time )
{
    TDateTime currDateTime;
    GetLocalTime_APDU( &currDateTime );

    if( cmpDateTimeStructs( time, &currDateTime ) )                              // compare time with current time
        return true;
    
    return false;
}

bool cmpDateTimeWithNotSpecifiedDateTime( const TDateTime* const time )
{
    TDateTime notSpecifiedDateTime = { {0xff, 0xff, 0xff, 0xff, 0xff}, {0xff, 0xff, 0xff, 0xff}, 0xff, 0xff };
    
    if( cmpDateTimeStructs( time, &notSpecifiedDateTime ) )                              // compare time with not specified time
        return true;
    
    return false;
}

/*
Functions compare two logical names.
Returns true if logical names are equal.
*/
bool cmpLN( const LOGICAL_NAME* ln1, const LOGICAL_NAME* ln2 )
{
    if( ln1->_A == ln2->_A &&
        ln1->_B == ln2->_B &&
        ln1->_C == ln2->_C &&
        ln1->_D == ln2->_D &&
        ln1->_E == ln2->_E &&
        ln1->_F == ln2->_F ) return true;
    return false;
}

/*
Functions search in the array of pointers to logical names (arrayOfLn) given logical name (ln).
Returns true if there is given ln in arrayOfLn. 
*/
bool findLnInArrayOfLn( const LOGICAL_NAME* arrayOfLn[], const LOGICAL_NAME* ln, u8 lenOfArray )
{
    for( u8 i = 0; i < lenOfArray; ++i )
    {
       if( cmpLN( arrayOfLn[i], ln ) )
       {
          return true;          // if there is ln in the arrayOfLn
       }
    }
    
    return false;               // if there isn't ln in the arrayOfLn
}

bool cmpIndex( const u8* index1, const u8* index2 )
{
    for( u8 i = 0; i < MAX_INDEX_LEN; ++i )
    {
        if( index1[i] != index2[i] )
            return false;
    }  
    return true;                                        // "indexes" are equal
}

bool cmpIndexWithZero( u8* index )
{
    for( u8 i = 0; i < MAX_INDEX_LEN; ++i )
    {
        if( index[i] != 0)
          return false;
    }
    return true;
}

s8 findIndex( const chargeTableElementType* elementList, const u8* index )
{
    for( u8 i = 0; i < MAX_TARIFFS; ++i )
    {
        if( cmpIndex( elementList[i].index, index ) )
          return i;                                     // return index in array elementList of "index" :)
    }
    return -1;                                          // "index" was not found
}

s8 lenChargeTableElement( const chargeTableElementType* elementList )
{
    s8 len = 0;
    for( int i = 0; i < MAX_TARIFFS; ++i )
    {
        if( elementList[i].chargePerUnit == 0 )
        {
            return len;
        }
        ++len;
    }
    
    return len;
}

s16 findChargePerUnit( const chargeTableElementType* elementList, const u8* index )
{
    s8 pos = findIndex( elementList, index );
    if( pos == -1 )
        return -32768;                                  // "index" was not found
    
    return elementList[pos].chargePerUnit;              // return charge_per_unit corresponding given "index"
}

bool cmpOctetStrings( const BYTE* const str1, const BYTE* const str2, u32 len )
{
    for( u32 i = 0; i < len; ++i )
    {
        if( str1[i] != str2[i] )
            return false;
    }
    return true;
}

void convertUnitChargeFromStructToMemoryBuffer( BYTE* dstBufUnitCharge, const PaymentChargeUnitCharge* const src )
{
    u8 posInBufUnitCharge = 0;
    
    dstBufUnitCharge[ posInBufUnitCharge++ ] = src->chargePerUnitScaling.commodityScale;
    dstBufUnitCharge[ posInBufUnitCharge++ ] = src->chargePerUnitScaling.priceScale;
    
    AXDREncodeWord( &dstBufUnitCharge[ posInBufUnitCharge ], src->commodityReference.classId );
    posInBufUnitCharge += eDTL_LongUnsigned;
    memcpy( &dstBufUnitCharge[ posInBufUnitCharge ], &src->commodityReference.logicalName, 6 );
    posInBufUnitCharge += 6;
    dstBufUnitCharge[ posInBufUnitCharge++ ] = src->commodityReference.attributeIndex;
    
    for( u8 i = 0; i < MAX_TARIFFS; ++i )
    {
        memcpy( &dstBufUnitCharge[ posInBufUnitCharge ], &src->chargeTableElement[i].index, MAX_INDEX_LEN );
        posInBufUnitCharge += MAX_INDEX_LEN;
        AXDREncodeShort( &dstBufUnitCharge[ posInBufUnitCharge ], src->chargeTableElement[i].chargePerUnit );
        posInBufUnitCharge += eDTL_Long;
    }
}

void convertUnitChargeFromMemoryBufferToStruct( const BYTE* const srcBufUnitCharge, PaymentChargeUnitCharge* const dst )
{
    u8 posInBufUnitCharge = 0;
    
    dst->chargePerUnitScaling.commodityScale = srcBufUnitCharge[ posInBufUnitCharge++ ];
    dst->chargePerUnitScaling.priceScale = srcBufUnitCharge[ posInBufUnitCharge++ ];
    
    AXDRDecodeWord( (BYTE*)&srcBufUnitCharge[ posInBufUnitCharge ], &dst->commodityReference.classId );
    posInBufUnitCharge += eDTL_LongUnsigned;
    memcpy( &dst->commodityReference.logicalName, &srcBufUnitCharge[ posInBufUnitCharge ], 6 );
    posInBufUnitCharge += 6;
    dst->commodityReference.attributeIndex = srcBufUnitCharge[ posInBufUnitCharge++ ];
    
    for( u8 i = 0; i < MAX_TARIFFS; ++i )
    {
        memcpy( &dst->chargeTableElement[i].index, &srcBufUnitCharge[ posInBufUnitCharge ], MAX_INDEX_LEN );
        posInBufUnitCharge += MAX_INDEX_LEN;
        AXDRDecodeShort( (BYTE*)&srcBufUnitCharge[ posInBufUnitCharge ], &dst->chargeTableElement[i].chargePerUnit );
        posInBufUnitCharge += eDTL_Long;
    }
}

/*
Function returns adjusted value according with scale.
scale - necessary value scale;
*/
s32 scaleValue( s32 value, s8 scale )
{
    if( value == 0 || scale == 0 )
        return value;
    
    if( scale > 0 )
    {
        for( u8 i = 0; i < scale; ++i )
        {
            value *= 10;
        }
    }
    else
    {
        for( u8 i = 0; i < (scale * -1); ++i )
        {
            value /= 10;
        }
    }
  
    return value;
}

s8 GetScalerOfValueFromRegister( const LOGICAL_NAME* const ln )
{
    /* Read and save scaler of value from register */
    BYTE bufValueScaler[7] = {};
    InternalGetRequest( RegisterClassID, ln, 3, NULL, bufValueScaler );
    if( bufValueScaler[0] != Success || bufValueScaler[1] != Structure || bufValueScaler[2] != 2 || bufValueScaler[3] != Integer || bufValueScaler[5] != Enum )
      return -128;                                                              // Error: scaler of value type from register is wrong
    
    return (s8)bufValueScaler[4];            
}

u64 GetValueFromRegister( const LOGICAL_NAME* const ln )
{
    /* Read and save value from register */
    BYTE bufValue[10] = {};
    InternalGetRequest( RegisterClassID, ln, 2, NULL, bufValue );
    if( bufValue[0] != Success || bufValue[1] != eDT_Long64Unsigned )
      return 0;                                                                 // Error: value type from register is wrong
    U64 value;
    AXDRDecodeULong64( &bufValue[2], (u8*)&value );
    
    return value;
}

/************************************************************************************************/
/************************ Function of Payment classes *******************************************/
/************************************************************************************************/

/*********************************************/
/******* Functions of PaymentAccountClass ****/
/*********************************************/

void PaymentAccountClass::ExecuteCollection( u8 chargeIndex )
{
    if( currValues.currCreditInUse >= lenCreditList )   // If there is not credit in use
    {
        chargeList[chargeIndex]->RefuseCollection();
        return;
    }
    
    if( !CheckCreditChargeConfiguration( chargeIndex ) )
    {
        chargeList[chargeIndex]->RefuseCollection();      
        return;
    }
    
    s32 tmpSumToCollect = chargeList[chargeIndex]->GetSumToCollect();
    s8 scaleOfTmpSumToCollect = chargeList[chargeIndex]->GetPriceScale();
    
    s8 commonScaler = scaleOfTmpSumToCollect - accountCfg->currency.scale;
            
    s32 sumToCollect = scaleValue( tmpSumToCollect, commonScaler) * -1;
    
    creditList[currValues.currCreditInUse]->UpdateAmount( sumToCollect );
    
    chargeList[chargeIndex]->ConfirmCollection();
}

/* The functions returns:
true if charge can collect from credit in_use;
false if charge cann't collect from credit in_use. */
bool PaymentAccountClass::CheckCreditChargeConfiguration( u8 chargeIndex )
{
    if( lenCreditChargeCfgList == 0 )
        return true;
    
    const LOGICAL_NAME* creditInUseLn = creditList[currValues.currCreditInUse]->GetLn();
    const LOGICAL_NAME* chargeLn = chargeList[chargeIndex]->GetLn();
    
    for( u8 i = 0; i < lenCreditChargeCfgList; ++i )
    {
        if( cmpLN( accountCfg->creditChargeCfg[i].creditRef, creditInUseLn ) &&
            cmpLN( accountCfg->creditChargeCfg[i].chargeRef, chargeLn ) )
        {
//            if( !(accountCfg->creditChargeCfg[i].collectionCfg & collectWhenSupplyDisconected) )
//            {
//                /* proveriti vkliu4eno li rele */
//                /* vikliu4eno - nelizea */
//            }
//              
//            if( !(accountCfg->creditChargeCfg[i].collectionCfg & collectInLoadLimitingPeriods) )
//            {
//                /* proveriti esle sei4as load limiting period */
//                /* esli load limiting period - nelizea */
//            }
//            
//            if( !(accountCfg->creditChargeCfg[i].collectionCfg & collectInFriendlyCreditPeriods) )
//            {
//                /* proveriti esle sei4as friendly credit period */
//                /* esli friendly credit period - nelizea */
//            }
              
            return true;
        }
    }
  
    return false;
}

void PaymentAccountClass::DistributeTopUpSumWithoutRestrictions( s32 topUpSum )
{
    /*
    Blue Book, p.188.
    Credit should be applied first to the lowest priority УCreditФ object with
    its credit_status set to (3) In use or (4) Exhausted.
    */
    for( s8 i = lenCreditList - 1; i >= 0; --i )
    {
        eT_creditStatus creditStatus = creditList[i]->GetCreditStatus();
        
        if( creditStatus == EXHAUSTED || creditStatus == IN_USE )
        {
            if( creditList[i]->GetCreditType() == emergencyCredit )
            {
                s32 presetCreditAmount = creditList[i]->GetPresetCreditAmount();
                s32 currentCreditAmount = creditList[i]->GetCurrentCreditAmount();
                /*
                Blue Book, p.180.
                Emergency credit can only receive credit amounts when it has been consumed (entirely or partially)
                and when credit_configuration has bit 2 (Requires the credit amount to be paid back) set.
                */
                if( currentCreditAmount < presetCreditAmount && creditList[i]->GetRequiresCreditAmountToBePaidBack() )
                {
                    s32 spentCredit = presetCreditAmount - currentCreditAmount;
                    if( spentCredit >= topUpSum )
                    {
                        creditList[i]->UpdateAmount( topUpSum );
                        topUpSum = 0;
                    }
                    else
                    {
                        creditList[i]->UpdateAmount( spentCredit );
                        topUpSum -= spentCredit;
                    }
                }
            }
            else
            {
                creditList[i]->UpdateAmount( topUpSum );
                topUpSum = 0;
            }
            
            if( topUpSum == 0 )
            {
                tokenGateway->ConfirmReceivedToken();
                break;
            }
        }
    }
    
    /*
    Blue book, p.188.
    Then apportioned to the other УCreditФ objects in ascending order of the
    УCreditФ object priority (starting with priority n and ending with priority 1).
    */
    if( topUpSum > 0 )  
    {
        for( s8 i = lenCreditList - 1; i >= 0; --i )
        {
            eT_creditStatus creditStatus = creditList[i]->GetCreditStatus();
            
            if( creditStatus == ENABLED || creditStatus == SELECTABLE || creditStatus == SELECTED )
            {
                if( creditList[i]->GetCreditType() == emergencyCredit )
                {
                    s32 presetCreditAmount = creditList[i]->GetPresetCreditAmount();
                    s32 currentCreditAmount = creditList[i]->GetCurrentCreditAmount();
                    /*
                    Blue Book, p.180.
                    Emergency credit can only receive credit amounts when it has been consumed (entirely or partially)
                    and when credit_configuration has bit 2 (Requires the credit amount to be paid back) set.
                    */
                    if( currentCreditAmount < presetCreditAmount && creditList[i]->GetRequiresCreditAmountToBePaidBack() )
                    {
                        s32 spentCredit = presetCreditAmount - currentCreditAmount;
                        if( spentCredit >= topUpSum )
                        {
                            creditList[i]->UpdateAmount( topUpSum );
                            topUpSum = 0;
                        }
                        else
                        {
                            creditList[i]->UpdateAmount( spentCredit );
                            topUpSum -= spentCredit;
                        }
                    }
                }
                else
                {
                    creditList[i]->UpdateAmount( topUpSum );
                    topUpSum = 0;
                }
                
                if( topUpSum == 0 )
                {
                    tokenGateway->ConfirmReceivedToken();
                    break;
                }
            }
        }
    }   
}

void PaymentAccountClass::DistributeTopUpSumAccordingToProportion( s32 topUpSum )
{
    s32 expendedFromTopUpSum = 0;
    /*
    Blue Book, p.188.
    This attribute determines the minimum proportion of the credit token that is
    attributed to each УCreditФ object referenced in the array.
    */    
    for( u8 i = 0; i < lenTokenGatewayCfgList; ++i )
    {
        for( u8 j = 0; j < lenCreditList; ++ j )
        {
            if( cmpLN( &accountCfg->tokenGatewayCfg[i].creditRef, creditList[j]->GetLn() ) )
            {
                s32 topUpSumForThisCredit = (topUpSum * accountCfg->tokenGatewayCfg[i].tokenProportion) / 100;
                
                creditList[j]->UpdateAmount( topUpSumForThisCredit );
                
                expendedFromTopUpSum += topUpSumForThisCredit;
            }
        }
    }
    
    topUpSum -= expendedFromTopUpSum;
        
    /*
    Blue Book, p.188.
    If the sum of the proportions is less than 100% then the remainder will be
    added to the УCreditФ objects using the rules below for an empty
    Уtoken_gateway_configurationФ attribute.
    */
    if( topUpSum > 0 )          // if smth remained in top up sum
    {
        DistributeTopUpSumWithoutRestrictions( topUpSum );
    }
    else
    {
        tokenGateway->ConfirmReceivedToken();
    }
    
    return;
}

void PaymentAccountClass::DistributeTopUpSumBetweenCredits( s32 topUpSum )
{
    if( lenTokenGatewayCfgList == 0 )
    {
        DistributeTopUpSumWithoutRestrictions( topUpSum );
    }
    else
    {
        DistributeTopUpSumAccordingToProportion( topUpSum );
    }
}

bool PaymentAccountClass::InvokeHighestPriorityCreditToInUse()
{
    for( u8 i = 0; i < lenCreditList; ++i )
    {
        if( creditList[i]->GetCreditStatus() != EXHAUSTED )
        {
            if( creditList[i]->InvokeCreditStatusToInUse() )
            {
                currValues.currCreditInUse = i;
                return true;
            }
        }
    }
    return false;

//    u8 nextPriorityCreditIndex = FindIndexOfNextPriorityCredit();
//    if( nextPriorityCreditIndex >= lenCreditList )      // if wasn't found next priority credit
//      return false;
//    
//    if( creditList[nextPriorityCreditIndex]->InvokeCreditStatusToInUse() )
//    {
//      currValues.currCreditInUse = nextPriorityCreditIndex;
//      return true;
//    }
//    return false;
}

void PaymentAccountClass::UpdateCurrentCreditStatus()
{
    /* Bit 0. in_credit */
    if( currValues.availableCredit > 0 )
        currValues.currCreditStatus |= creditStatusInCredit;
    else
        currValues.currCreditStatus &= ~creditStatusInCredit;
    
    /* Bit 1. low_credit */
    if( currValues.availableCredit < currValues.lowCreditThreshold )
        currValues.currCreditStatus |= creditStatusLowCredit;
    else
        currValues.currCreditStatus &= ~creditStatusLowCredit;
     
    u8 indexNextPriorityCredit = FindIndexOfNextPriorityCredit();
    eT_creditStatus nextPriorityCreditStatus = creditList[indexNextPriorityCredit]->GetCreditStatus();
    
    /* Bit 2. next_credit_enabled; Bit 3. next_credit_selectable; Bit 2. next_credit_selected;*/
    if( indexNextPriorityCredit >= lenCreditList )   /* If there are not next priority credit */
    {
        currValues.currCreditStatus &= ~creditStatusNextCreditEnabled;
        currValues.currCreditStatus &= ~creditStatusNextCreditSelectable;
        currValues.currCreditStatus &= ~creditStatusNextCreditSelected;
    }
    else
    {     
        currValues.currCreditStatus &= ~creditStatusNextCreditEnabled;
        currValues.currCreditStatus &= ~creditStatusNextCreditSelectable;
        currValues.currCreditStatus &= ~creditStatusNextCreditSelected;
        
        if( nextPriorityCreditStatus == ENABLED )
            currValues.currCreditStatus |= creditStatusNextCreditEnabled;
        else if( nextPriorityCreditStatus == SELECTABLE )
            currValues.currCreditStatus |= creditStatusNextCreditSelectable;
        else if( nextPriorityCreditStatus == SELECTED )
            currValues.currCreditStatus |= creditStatusNextCreditSelected;
    }
    
    /* Bit 5. selectable_credit_in_use */
    /* Set and Clear in the ManageCreditsStatuses function */
    
    /* Bit 6. out_of_credit */
    if( creditList[currValues.currCreditInUse]->GetCreditStatus() == EXHAUSTED &&
       (indexNextPriorityCredit >= lenCreditList || nextPriorityCreditStatus == SELECTABLE) )
        currValues.currCreditStatus |= creditStatusOutOfCredit;
    else
        currValues.currCreditStatus &= ~creditStatusOutOfCredit;
}

void PaymentAccountClass::UpdateAvailableCredit()
{
    currValues.availableCredit = 0;
    
    for( u8 i = 0; i < lenCreditList; ++i )
    {
        eT_creditStatus creditStatus = creditList[i]->GetCreditStatus();
        if( creditStatus == SELECTED ||
            creditStatus == IN_USE )
        {
            s32 currCreditAmount = creditList[i]->GetCurrentCreditAmount();
            if( currCreditAmount > 0 )
                currValues.availableCredit += currCreditAmount;
        }
    }
}

void PaymentAccountClass::UpdateAmountToClear()
{
    currValues.amountToClear = 0;
    
    for( u8 i = 0; i < lenCreditList; ++i )
    {
        s32 currCreditAmount = creditList[i]->GetCurrentCreditAmount();
        if( creditList[i]->GetCreditStatus() == EXHAUSTED &&
            !(creditList[i]->GetRequiresCreditAmountToBePaidBack()) )
        {            
            if( currCreditAmount < 0 )
                currValues.amountToClear += currCreditAmount;
        }
        else if( creditList[i]->GetRequiresCreditAmountToBePaidBack() )
        {
            currValues.amountToClear += currCreditAmount * -1;
        }
    }
    currValues.amountToClear += accountCfg->clearanceThreshold * -1;
}

void PaymentAccountClass::UpdateAggregatedDebt()
{
    currValues.aggregatedDebt = 0;
   
    for( u8 i = 0; i < lenChargeList; ++i )
    {
        if( !(chargeList[i]->GetContinuousCollection()) )
        {
            s32 tmpTotalAmountRemaining = chargeList[i]->GetTotalAmountRemaining();
            s8 scaleOfTmpTotalAmountRemainig = chargeList[i]->GetPriceScale();
            
            s8 commonScaler = scaleOfTmpTotalAmountRemainig - accountCfg->currency.scale;
            
            tmpTotalAmountRemaining = scaleValue( tmpTotalAmountRemaining, commonScaler );
            
            currValues.aggregatedDebt += tmpTotalAmountRemaining;
        }
    }
}

void PaymentAccountClass::UpdateLowCreditThreshold()
{
    if( currValues.currCreditInUse >= lenCreditList )   // If no credit in use
    {
        currValues.lowCreditThreshold = 0;
        return;
    }
    
    currValues.lowCreditThreshold = creditList[currValues.currCreditInUse]->GetWarningThreshold();
}

void PaymentAccountClass::UpdateNextCreditAvailableThreshold()
{
    u8 indexNextPriorityCredit = FindIndexOfNextPriorityCredit();
    if( indexNextPriorityCredit >= lenCreditList )   // if wasn't found next priority credit
    {
        currValues.nextCreditAvailableThreshold = -2147483648;  /* According to BlueBook->Account->next_credit_available_threshold */
    }
    else
    {
        currValues.nextCreditAvailableThreshold = creditList[indexNextPriorityCredit]->GetCreditAvailableThreshold();
    }
}

void PaymentAccountClass::ManageCreditsStatuses()
{
    u8 nextPriorityCreditIndex = FindIndexOfNextPriorityCredit();
    if( nextPriorityCreditIndex >= lenCreditList )                              // if wasn't found next priority credit
        return;
    
    if( nextPriorityCreditIndex < currValues.currCreditInUse )                  // if the next priority credit has highest priority than the current credit
    {
        if( creditList[nextPriorityCreditIndex]->InvokeCreditStatusToInUse() )  // invoke next priority credit to in_use
        {
            for( u8 i = 0; i < lenCreditList; ++i )
            {
                if( i == currValues.currCreditInUse )
                    continue;
                
                creditList[i]->InvokeCreditStatusToEnable();                    // invoke all other credits from selectable/selected/in_use to enable (H. in_use->enable)
            }
            
            currValues.currCreditStatus &= ~creditStatusSelectableCreditInUse;  /* Clear Bit 5. selectable_credit_in_use */
            
            return;
        }
    }
    
    if( currValues.availableCredit <= currValues.nextCreditAvailableThreshold && currValues.nextCreditAvailableThreshold > 0)   // A. enable->selectable; J. enable->selected
    {
        creditList[nextPriorityCreditIndex]->InvokeCredit();
        return;
    }
    
    if( currValues.availableCredit <= currValues.nextCreditAvailableThreshold && currValues.nextCreditAvailableThreshold <= 0 ) // C. selected->in_use
    {
        eT_creditStatus nextPriorityCreditStatus = creditList[nextPriorityCreditIndex]->GetCreditStatus();
        if( creditList[nextPriorityCreditIndex]->InvokeCreditStatusToInUse() )
        {
            if( nextPriorityCreditStatus == SELECTED )
                currValues.currCreditStatus |= creditStatusSelectableCreditInUse;       /* Set Bit 5. selectable_credit_in_use */
          
            creditList[currValues.currCreditInUse]->InvokeCreditStatusToEnable();
            currValues.currCreditInUse = nextPriorityCreditIndex;
            return;
        }
    }
    
    if( creditList[currValues.currCreditInUse]->GetCreditStatus() == EXHAUSTED )  // I. enable->in_use
    {
        eT_creditStatus nextPriorityCreditStatus = creditList[nextPriorityCreditIndex]->GetCreditStatus();
        if( creditList[nextPriorityCreditIndex]->InvokeCreditStatusToInUse() )
        {
            if( nextPriorityCreditStatus == SELECTED )
                currValues.currCreditStatus |= creditStatusSelectableCreditInUse;       /* Set Bit 5. selectable_credit_in_use */     
          
            currValues.currCreditInUse = nextPriorityCreditIndex;
            return;
        }
    }
    
    if( creditList[nextPriorityCreditIndex]->GetCreditStatus() == SELECTABLE )          // F. selectable->enable
    {
        if( currValues.availableCredit > currValues.nextCreditAvailableThreshold )
        {
            creditList[nextPriorityCreditIndex]->InvokeCreditStatusToEnable();
            return;
        }
    }
    else if( creditList[nextPriorityCreditIndex]->GetCreditStatus() == SELECTED )       // G. selected->enable
    {
        /* This is done such way to avoid rattling (Blue Book p.201) */
        if( currValues.availableCredit - creditList[nextPriorityCreditIndex]->GetCurrentCreditAmount() > currValues.nextCreditAvailableThreshold )
        {
            creditList[nextPriorityCreditIndex]->InvokeCreditStatusToEnable();
            return;
        }
    }
}

void PaymentAccountClass::ActivateLinkedCharges() const
{
    for( u8 i = 0; i < lenChargeList; ++i )
    {
        chargeList[i]->ActivateCharge();
    }
}

void PaymentAccountClass::CloseLinkedCharges() const
{
    for( u8 i = 0; i < lenChargeList; ++i )
    {
        chargeList[i]->CloseCharge();
    }
}

/*
The function returns lenCreditList if wasn't found next priority credit
*/
u8 PaymentAccountClass::FindIndexOfNextPriorityCredit() const
{
    for( u8 i = 0; i < lenCreditList; ++i )
    {
        eT_creditStatus creditStatus = creditList[i]->GetCreditStatus();
        if( creditStatus == ENABLED ||
            creditStatus == SELECTABLE ||
            creditStatus == SELECTED )
        {
            return i;
        }
    }
    
    return lenCreditList;          // wasn't found next priority credit
}
            
void PaymentAccountClass::ActivateAccount( s32 data )                       // done
{
    if( accountCfg->modeAndStatus.accountStatus == newAccount )
    {
        accountCfg->modeAndStatus.accountStatus = activeAccount;
        FileWrite( ftFile->ftAccountStatus, &accountCfg->modeAndStatus.accountStatus );
        
        GetLocalTime_APDU( &accountCfg->accountActivationTime );
        DWORD seconds = PackTdateTime( &accountCfg->accountActivationTime );        
        FileWrite( ftFile->ftAccountActivationTime, &seconds );
        
        ActivateLinkedCharges();
    }
    return;
}

void PaymentAccountClass::CloseAccount( s32 data )                          // done          
{
    if( accountCfg->modeAndStatus.accountStatus == activeAccount )
    {
      accountCfg->modeAndStatus.accountStatus = closedAccount;
      FileWrite( ftFile->ftAccountStatus, &accountCfg->modeAndStatus.accountStatus );
      
      GetLocalTime_APDU( &accountCfg->accountClosureTime );
      DWORD seconds = PackTdateTime( &accountCfg->accountClosureTime );        
      FileWrite( ftFile->ftAccountClosureTime, &seconds );
      
      CloseLinkedCharges();
    }
    return;
}

/* This function may be specific from project to project. */
void PaymentAccountClass::ResetAccount( s32 data )                          // done
{
    if( accountCfg->modeAndStatus.accountStatus != newAccount )
    {        
        for( u8 i = 0; i < lenChargeList; ++i )
            chargeList[i]->ResetCharge();
        
        for( u8 i = 0; i < lenCreditList; ++i )
            creditList[i]->ResetCredit();   
        
        accountCfg->modeAndStatus.accountStatus = newAccount;
        currValues.currCreditInUse = lenCreditList;
        currValues.availableCredit = 0;
        currValues.amountToClear = 0;
        currValues.aggregatedDebt = 0;
        currValues.lowCreditThreshold = 0;
        currValues.nextCreditAvailableThreshold = 0;
        
        FileWrite( ftFile->ftAccountStatus, &accountCfg->modeAndStatus.accountStatus );
    }
  
    return;
}

void PaymentAccountClass::TopUpCredits( s32 topUpSum )
{
    DistributeTopUpSumBetweenCredits( topUpSum );
    for( u8 i = 0; i < lenChargeList; ++i )
    {
      chargeList[i]->ExecutePaymentEventBasedCollection( topUpSum );
    }
}

void PaymentAccountClass::IdleSecond()
{
//    if(  tokenGateway != nullptr )  // if there is linked token gateway object
//    {
//        if( tokenGateway->GetNewTokenTopUp() )
//        {
//            s32 topUpSum = tokenGateway->GetTopUpSum();
//            DistributeTopUpSumBetweenCredits( topUpSum );
//            for( u8 i = 0; i < lenChargeList; ++i )
//            {
//                chargeList[i]->ExecutePaymentEventBasedCollection( topUpSum );
//            }
//        }
//    }
  
    if( accountCfg->modeAndStatus.accountStatus == activeAccount )
    {
        if( currValues.currCreditInUse >= lenCreditList )   // If there is not credit in use
        {
            InvokeHighestPriorityCreditToInUse();
        }
        
        for( u8 i = 0; i < lenChargeList; ++i )
        {
            if( chargeList[i]->GetNewCollection() )
            {
                ExecuteCollection( i );
            }
        }
        
        UpdateAvailableCredit();
        UpdateAmountToClear();
        UpdateAggregatedDebt();
        UpdateLowCreditThreshold();
        UpdateNextCreditAvailableThreshold();
        UpdateCurrentCreditStatus();
        
        ManageCreditsStatuses();
        
        /* Described in Blue Book. Credit - warning_threshold */
//        if( currValues.currCreditStatus & creditStatusLowCredit )
//        {
//          // вызвать предупреждени€ дл€ пользовател€, что низкий кредит
//        }
        
        if( currValues.currCreditStatus & creditStatusOutOfCredit )
        {
            // OTKLIU4ITI OSNOVNOE RELE
            // esli ne "friendly hours" i td
            if( cicDisconnectorControlBase.GetOutputState() )
            {
                cicDisconnectorControlBase.ActionLocalDisconnect();

                currValues.currCreditInUse = lenCreditList;
            }
        }
        else
        {
            if( !cicDisconnectorControlBase.GetOutputState() )
            {
                cicDisconnectorControlBase.ActionLocalReconnect();
            }
        }
    }
    return;
}

void PaymentAccountClass::IdleMinute()
{  
    if( accountCfg->modeAndStatus.accountStatus == newAccount )
    {
        if( !cmpDateTimeWithNotSpecifiedDateTime( &accountCfg->accountActivationTime) )     // A definition with "not specified" notation in all fields of the attribute will not be acted upon.
        {
            if( cmpDateTimeWithCurrDateTime( &accountCfg->accountActivationTime ) )
            {
                ActivateAccount();
            }
        }
    }   
    else if( accountCfg->modeAndStatus.accountStatus == activeAccount )
    {
        if( !cmpDateTimeWithNotSpecifiedDateTime( &accountCfg->accountClosureTime ) )       // A definition with "not specified" notation in all fields of the attribute will not be acted upon.
        {        
            if( cmpDateTimeWithCurrDateTime( &accountCfg->accountClosureTime ) )
            {
                CloseAccount();
            }
        }
    }
    
    return;
}

void PaymentAccountClass::Init()
{
//    FlashFormat();
//    FramFormat();
  
    if( FileRead( ftFile->ftAccountStatus, &accountCfg->modeAndStatus.accountStatus ) == 0 )
    {
        accountCfg->modeAndStatus.accountStatus = newAccount;
    }
    else
    {
        for( u8 i = 0; i < lenChargeList; ++i )
            chargeList[i]->SetIsLinkedAccountActive( (accountCfg->modeAndStatus.accountStatus == activeAccount) ? true : false );
    }
  
    DWORD seconds = 0;
    if( FileRead( ftFile->ftAccountActivationTime, &seconds ) == eDTL_DoubleLongUnsigned ) /* if in the flash was written account_activation_time */
    {
        UnpackTDateTime( seconds, &accountCfg->accountActivationTime );
    }   
    else
    {
        accountCfg->accountActivationTime = { {0xff, 0xff, 0xff, 0xff, 0xff}, {0xff, 0xff, 0xff, 0xff}, 0xff, 0xff, {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff} };
    }
    
    seconds = 0;
    if( FileRead( ftFile->ftAccountClosureTime, &seconds ) == eDTL_DoubleLongUnsigned )   /* if in the flash was written account_closure_time */
    {
        UnpackTDateTime( seconds, &accountCfg->accountClosureTime );
    }  
    else
    {
        accountCfg->accountClosureTime = { {0xff, 0xff, 0xff, 0xff, 0xff}, {0xff, 0xff, 0xff, 0xff}, 0xff, 0xff, {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff} };
    }
    
    BYTE bufFromFlashForCurrency[MAX_LEN_CURRENCY_NAME + eDTL_Integer + eDTL_Enum] = {};
    if( FileRead(ftFile->ftCurrency, bufFromFlashForCurrency) == (MAX_LEN_CURRENCY_NAME + eDTL_Integer + eDTL_Enum) )
    {
        memcpy( &accountCfg->currency.name, &bufFromFlashForCurrency[0], MAX_LEN_CURRENCY_NAME );
        accountCfg->currency.scale = bufFromFlashForCurrency[3];
        accountCfg->currency.unit = static_cast<eT_currency_unit>(bufFromFlashForCurrency[4]);
    }
    
    u16 tmpMaxProvision = 0;
    if( FileRead( ftFile->ftMaxProvision, &tmpMaxProvision ) != 0 )
    {
        accountCfg->maxProvision = tmpMaxProvision;
    }
    
    s32 tmpMaxProvisionPeriod = 0;
    if( FileRead( ftFile->ftMaxProvision, &tmpMaxProvisionPeriod ) != 0 )
    {
        accountCfg->maxProvisionPeriod = tmpMaxProvisionPeriod;
    }
    
    currValues.currCreditInUse = lenCreditList; // it means that at the moment no credit in use. Need to determine it later - in IdleSecond.
    
    return;
}

PaymentAccountClass::PaymentAccountClass( const LOGICAL_NAME* const _ln,
                                         PaymentAccountCfg* const _cfg,
                                         PaymentCreditClass* const _creditList[],
                                         PaymentChargeClass* const _chargeList[],
                                         PaymentTokenGatewayClass* const _tokenGateway,
                                         const ftPaymentAccount* const _ftFile ) : ln( _ln ), accountCfg( _cfg ), tokenGateway( _tokenGateway ), ftFile( _ftFile ) {    
#ifndef NEW_CONST_CLASS_MAP
     //DataObjectsMap[(LOGICAL_NAME*)_ln] = this;                                      
     PaymentAccountObjectsMap[(LOGICAL_NAME*)_ln] = this;
#endif // NEW_CONST_CLASS_MAP
    
    /* copy pointers to creditList from _creditList */
    lenCreditList = MAX_OBJECTS_IN_CREDIT_REF_LIST;
    for( u8 i = 0; i < MAX_OBJECTS_IN_CREDIT_REF_LIST; ++i )
    {
        if( _creditList[i] == nullptr )
        {
            lenCreditList = i;
            break;
        }
        else
        {
            creditList[i] = _creditList[i];
        }
    }
    /* copy pointers to chargeList from _chargeList */
    lenChargeList = MAX_OBJECTS_IN_CHARGE_REF_LIST;
    for( u8 i = 0; i < MAX_OBJECTS_IN_CHARGE_REF_LIST; ++i )
    {
        if( _chargeList[i] == nullptr )
        {
            lenChargeList = i;
            break;
        }
        else
        {
            chargeList[i] = _chargeList[i];
        }
    }
    
    lenCreditChargeCfgList = 0;
    while( accountCfg->creditChargeCfg[lenCreditChargeCfgList].creditRef != NULL )
    {
        ++lenCreditChargeCfgList;
        if( lenCreditChargeCfgList == MAX_OBJECTS_IN_CREDIT_CHARGE_CFG )
            break;
    }
    
    lenTokenGatewayCfgList = 0;
    while( accountCfg->tokenGatewayCfg[lenTokenGatewayCfgList].creditRef._F != 0 )
    {
        ++lenTokenGatewayCfgList;
        if( lenTokenGatewayCfgList == MAX_OBJECTS_IN_TOKEN_GATEWAY_CFG )
            break;
    }
}

/*********************************************/
/******* Functions of PaymentCreditClass *****/
/*********************************************/

/*
Funtion control the credit status if it is IN_USE or EXHAUSTED.
The function watches only for D and E transitions from the Table 29.
Other transitions are watched for in PaymentManagerClass::controlCreditStatus().
*/
void PaymentCreditClass::ControlCreditStatus()
{
    if( currValues.creditStatus == IN_USE )
    {
        if( currValues.currentCreditAmount <= creditCfg->limit )
        {
            currValues.creditStatus = EXHAUSTED;        // In use -> Exhausted (Table 29, D)
        }
    }
    else if( currValues.creditStatus == EXHAUSTED )
    {
        if( currValues.currentCreditAmount > creditCfg->limit )
        {
            currValues.creditStatus = ENABLED;          // Exhausted -> Enabled (Table 29, E)
        }
    }
    
    return;
}

bool PaymentCreditClass::CheckPeriod()
{
    TDateTime currDateTime;    
    GetLocalTime_APDU( &currDateTime );
    
    if( CmpDateTimeWc( &creditCfg->period, &currDateTime, 0, 0 ) )
        return true;
    
    return false;
}

void PaymentCreditClass::UpdateAmount( s32 value )              // done
{
    currValues.currentCreditAmount += value;
    FileWrite( ftFile->ftCurrentCreditAmount, &currValues.currentCreditAmount );
    
    ControlCreditStatus();
    
    return;
}

s32 PaymentCreditClass::SetAmountToValue( s32 newValue )        // done
{
    s32 previousCreditAmount = currValues.currentCreditAmount;
    currValues.currentCreditAmount = newValue;
    FileWrite( ftFile->ftCurrentCreditAmount, &currValues.currentCreditAmount );
    
    return previousCreditAmount;
}

void PaymentCreditClass::InvokeCredit( s32 data )           // done                      
{
    if( currValues.creditStatus == ENABLED )                            // only if credit_status is ENABLED
    {
        if( creditCfg->creditConfiguration & creditCfgConfirmation )    // if need confirmation
        {
            currValues.creditStatus = SELECTABLE;                       // credit_status changes to SELECTABLE
        }
        else                                                            // if need not confirmation
        {
            currValues.creditStatus = SELECTED;                         // credit_status changes to SELECTED
        }
        FileWrite( ftFile->ftCreditStatus, &currValues.creditStatus );
    }
    return;
}

void PaymentCreditClass::IdleSecond()
{
    ControlCreditStatus();
  
//    if( currValues.creditStatus == IN_USE )
//    {
//        /* Described in Blue Book. Credit - credit_configuration */
//        if( creditCfg->creditConfiguration & creditCfgVisualInd )
//        {
//            // отображать на дисплее кредит
//        }
//        /* Described in Blue Book. Credit - preset_credit_amount*/
//        if( creditCfg->creditType == EmergencyCredit )
//        {
//            if( (currValues.creditStatus == IN_USE || currValues.creditStatus == EXHAUSTED) &&
//                 creditCfg->creditConfiguration & creditCfgRepayment )                          // some (or all) credit has been used; - осталось не пон€тным дл€ мен€
//              creditCfg->creditConfiguration |= creditCfgReceiveCreditToken;
//        }
//
//        /* Described in Blue Book. Credit - preset_credit_amount*/
//        if( creditCfg->presetCreditAmount != 0 )
//        {
//            if( ( currValues.creditStatus == SELECTED && creditCfg->creditConfiguration & creditCfgConfirmation ) ||
//                ( currValues.creditStatus == IN_USE && !(creditCfg->creditConfiguration & creditCfgConfirmation) ) ||
//                ( currValues.creditStatus == SELECTABLE && creditCfg->creditConfiguration & creditCfgConfirmation ) )
//              UpdateAmount( creditCfg->presetCreditAmount );
//        }
//    }
  
    return;
}

void PaymentCreditClass::IdleMinute()
{
    /* Described in Blue Book. Credit - period*/
    if( creditCfg->creditType == timeBasedCredit || creditCfg->creditType == consumptionBasedCredit )
    {
        if( CheckPeriod() )
            SetAmountToValue( creditCfg->presetCreditAmount );
    }
    
    return;
}

void PaymentCreditClass::Init()
{
    FileRead( ftFile->ftCurrentCreditAmount, &currValues.currentCreditAmount );    
    
    s32 tmpWarningThreshold = 0;
    if( FileRead( ftFile->ftWarningThreshold, &tmpWarningThreshold ) != 0 )
    {
        creditCfg->warningThreshold = tmpWarningThreshold;
    }
    
    s32 tmpLimit = 0;
    if( FileRead( ftFile->ftLimit, &tmpLimit ) != 0 )
    {
        creditCfg->limit = tmpLimit;
    }
    
    if( FileRead( ftFile->ftCreditStatus, &currValues.creditStatus ) == 0 )     /* if credit_status wasn't save */
    {
        if( currValues.currentCreditAmount > creditCfg->limit )
            currValues.creditStatus = ENABLED;
        else
            currValues.creditStatus = EXHAUSTED;
    }
    
    return;
}

PaymentCreditClass::PaymentCreditClass( const LOGICAL_NAME* const _ln,
                                       PaymentCreditCfg* const cfg,
                                       const ftPaymentCredit* const _ftFile ) : ln(_ln), creditCfg(cfg), ftFile( _ftFile )
{
#ifndef NEW_CONST_CLASS_MAP
    //DataObjectsMap[(LOGICAL_NAME*)_ln] = this; 
    PaymentCreditObjectsMap[(LOGICAL_NAME*)ln] = this;  
#endif // NEW_CONST_CLASS_MAP
}

/*********************************************/
/******* Functions of PaymentChargeClass *****/
/*********************************************/
s32 PaymentChargeClass::UpdateTotalAmountPaid( s32 collectionValue )
{   
    s32 previousTotalAmountPaid = currValues.totalAmountPaid;   
    currValues.totalAmountPaid += collectionValue;  
    FileWrite( ftFile->ftTotalAmountPaid, &currValues.totalAmountPaid );   
    
    return previousTotalAmountPaid;
}

void PaymentChargeClass::UpdateLastCollectionTime()
{      
    GetLocalTime_APDU( &currValues.lastCollectionTime );   
  
    u32 seconds = PackTdateTime( &currValues.lastCollectionTime );
    FileWrite( ftFile->ftLastCollectionTime, &seconds );
    
    return;
}

void PaymentChargeClass::UpdateLastCollectionAmount( s32 sum )
{
    currValues.lastCollectionAmount = sum;    
    FileWrite( ftFile->ftLastCollectionAmount, &currValues.lastCollectionAmount );
    
    return;
}

/* Esli summa sbora budet previ6ati total_amount_remainig, to functiea umeni6it summu sbora */
s32 PaymentChargeClass::ReduceTotalAmountRemaining( s32 sum )
{
    if( sum < 0 )
        sum *= -1;
    
    currValues.totalAmountRemaining -= sum;
    
    if( currValues.totalAmountRemaining < 0 )
    {
        sum += currValues.totalAmountRemaining;
        currValues.totalAmountRemaining = 0;
    }
    
    FileWrite( ftFile->ftTotalAmountRemaining, &currValues.totalAmountRemaining );
    
    return sum;
}


void PaymentChargeClass::UpdateUnitCharge( chargeTableElementType* elements )           // done. mojno optimizirovati + dlina massiva, kotorii nado zapisati, budet izvestna 
{
    for ( u8 i = 0; i < MAX_TARIFFS; ++i )
    {
        for( u8 j = 0; j < MAX_TARIFFS; ++j )
        {
            if ( cmpIndexWithZero( chargeCfg->unitChargePassive.chargeTableElement[j].index ) )         // if no entry
            {
                for( ; j < MAX_TARIFFS; ++j )                                                           // then write all remain elements
                {
                      chargeCfg->unitChargePassive.chargeTableElement[j] = elements[i];
                      ++i;
                }
                break;
            }
            else if( cmpIndex( elements[i].index, chargeCfg->unitChargePassive.chargeTableElement[j].index ) )          // if indexes are equal
            {  
                chargeCfg->unitChargePassive.chargeTableElement[j].chargePerUnit = elements[i].chargePerUnit;           // then rewrite value
                break;                                                                                                  // and go to the next element
            }
        }
    }
    
    /* save in the flash the passive_unit_charge */
    const u8 lenBufUnitCharge = eDTL_Integer + eDTL_Integer + eDTL_LongUnsigned + 6 + eDTL_Integer + MAX_TARIFFS * ( MAX_INDEX_LEN + eDTL_Long );
    BYTE bufUnitCharge[ lenBufUnitCharge ] = {};    
    convertUnitChargeFromStructToMemoryBuffer( bufUnitCharge, &chargeCfg->unitChargePassive );
    FileWrite( ftFile->ftUnitChargePassive, bufUnitCharge );
}

void PaymentChargeClass::ActivatePassiveUnitCharge( s32 data = 0 )      // done
{
    chargeCfg->unitChargeActive = chargeCfg->unitChargePassive;
    
    /* save in the flash the active_unit_charge */
    const u8 lenBufUnitCharge = eDTL_Integer + eDTL_Integer + eDTL_LongUnsigned + 6 + eDTL_Integer + MAX_TARIFFS * ( MAX_INDEX_LEN + eDTL_Long );
    BYTE bufUnitCharge[ lenBufUnitCharge ] = {};    
    convertUnitChargeFromStructToMemoryBuffer( bufUnitCharge, &chargeCfg->unitChargeActive );    
    FileWrite( ftFile->ftUnitChargeActive, bufUnitCharge );
    
    return;
}

void PaymentChargeClass::Collect( s32 data = 0 )        // !!! TO DO according to charge->proportion !!!             
{
    if( chargeCfg->chargeType == PaymentChargeConsumptionBased )
      return;

    if( !(chargeCfg->chargeConfiguration & chargePercentageBaseCollection) )
    {
        newCollection = true;
        sumToCollect = chargeCfg->unitChargeActive.chargeTableElement[0].chargePerUnit;
    }
}

s32 PaymentChargeClass::UpdateTotalAmountRemaining( s32 valueToAdd )    // done
{    
    if( chargeCfg->chargeConfiguration & chargeContinuousCollection )   // if charge_configuration bit 1 (Continuous collection) is set, TotalAmountRemaining may not be changed .
        return currValues.totalAmountRemaining;
    
    s32 previousTotalAmountRemaining = currValues.totalAmountRemaining;
    
    currValues.totalAmountRemaining += valueToAdd;
    
    if( currValues.totalAmountRemaining < 0 )
        currValues.totalAmountRemaining = 0;
    
    FileWrite( ftFile->ftTotalAmountRemaining, &currValues.totalAmountRemaining );
    
    return previousTotalAmountRemaining;
}

s32 PaymentChargeClass::SetTotalAmountRemaining( s32 newValue )         // done
{
    if( newValue < 0 ) return -1;                                       // newValue must be > 0
    
    if( chargeCfg->chargeConfiguration & chargeContinuousCollection )   // if charge_configuration bit 1 (Continuous collection) is set, total_amount_remainig must be 0.
    {
        currValues.totalAmountRemaining = 0;
        return 0;
    }
    
    s32 previousTotalAmountRemaining = currValues.totalAmountRemaining;
    currValues.totalAmountRemaining = newValue;
    
    FileWrite( ftFile->ftTotalAmountRemaining, &currValues.totalAmountRemaining );
    
    return previousTotalAmountRemaining;
}

s16 PaymentChargeClass::GetCurrentChargePerUnit() const
{                  
    /* If the first elemet in the array has empty index that means that always one tarrif */
    if( chargeCfg->unitChargeActive.chargeTableElement[0].index[0] == 0x00 )
        return chargeCfg->unitChargeActive.chargeTableElement[0].chargePerUnit;
    
    u8 activeIndex[MAX_INDEX_LEN] = {0};        // !!! FOR TESTING !!!
    /* !!!
    прочитать от куда-то index и найти по нему цену в unit_charge_active
    предусмотреть что тарифф может быть разным в течение периода
    !!! */
    
    for( u8 i = 0; i < MAX_TARIFFS; ++i )
    {
        if( cmpOctetStrings( activeIndex, &chargeCfg->unitChargeActive.chargeTableElement[i].index[0], MAX_INDEX_LEN ) )
            return chargeCfg->unitChargeActive.chargeTableElement[i].chargePerUnit;
    }
    
    return 0;   // index was not found in unit_charge_active.charge_table_element
}

//u32 PaymentChargeClass::GetUnitsConsumedFromLastCollection() const
//{
//    BYTE buf[10] = {};
//    
//    /* Read and save value from register */
//    InternalGetRequest( chargeCfg->unitChargeActive.commodityReference.classId, &chargeCfg->unitChargeActive.commodityReference.logicalName, 2, NULL, buf );
//    if( buf[0] != Success || buf[1] != eDT_DoubleLongUnsigned )
//        return 0;                                                               // error geting value
//    U32 value;
//    AXDRDecodeDword( &buf[2], &value );
//    
//    if( lastValue[0 /* !!! index tarrifa !!! */ ] > value )
//        return 0;
//    
//    u32 difference = value - lastValue[0 /* !!! index tarrifa !!! */ ];
//    s8 commonScaler = valueScaler + chargeCfg->unitChargeActive.chargePerUnitScaling.commodityScale;
//    
//    u32 unitsConsumed = scaleValue( difference, commonScaler );                 // we found how much units of smth (ex. kWh) were consumed (without fractional part)
// 
//    return unitsConsumed;
//}


/* 
Function returns how much periods have passed till last collection time to current time.
*/
u32 PaymentChargeClass::CalcPeriodPassed() const
{
    if( cmpDateTimeWithNotSpecifiedDateTime(&currValues.lastCollectionTime) )
        return 0;
      
    TDateTime currDateTime;
    GetLocalTime_APDU( &currDateTime );
    
    s32 secondsPast = ClockCompareDateTime( &currDateTime, &currValues.lastCollectionTime );
    if( secondsPast < 0 )                                                       // if clock wrong
        return 0;
    
    return secondsPast / chargeCfg->period;
}

void PaymentChargeClass::ExecuteConsumptionBasedCollection()
{
    if( chargeCfg->period != 0 )
    {
        u32 periodCounter = CalcPeriodPassed();  /* Getting how much periods has past since last_collection_time */
        if( periodCounter == 0 )                                                
        {
            return;                                                             // Error: wrong clock!
        }            
        else
        {            
            s16 chargePerUnit = GetCurrentChargePerUnit();
            if( chargePerUnit == 0 )
                return;                                                         // Error: chargePerUnit was not found!    
            
            /* !!!!!!! FOR TESTING. nado ot kuda to 4itati index teku6ego tarifa !!!!!!! */
            u8 currentTariffIndex = 0;
            
            /* !!! Nado 4itati iz tariffnogo registra !!! */                                                       
            s8 valueScaler = GetScalerOfValueFromRegister( &chargeCfg->unitChargeActive.commodityReference.logicalName );
            if( valueScaler == -128 )
                return;                                                         // Error: scaler of value type from register is wrong
            
            /* !!! Nado 4itati iz tariffnogo registra !!! */                                                        
            U64 value = GetValueFromRegister( &chargeCfg->unitChargeActive.commodityReference.logicalName );
            
            if( lastValue[currentTariffIndex] >= value )
                return;                                                         // Error: value from register is wrong
            
            u32 difference = value - lastValue[ currentTariffIndex ];
            s8 commonScaler = valueScaler + chargeCfg->unitChargeActive.chargePerUnitScaling.commodityScale;
    
            u32 unitsConsumed = scaleValue( difference, commonScaler );         // we found how much units of smth (ex. kWh) were consumed (without fractional part)
            if( unitsConsumed == 0 )
                return;
            
            lastValue[ currentTariffIndex ] = value;                        
            lastValue[ currentTariffIndex ] -= difference % scaleValue( unitsConsumed, commonScaler * -1 );;    // vi4etaem iz lastValue drobnuiu (neu4tionnuiu pri ras4iote sumToCollect) 4asti, 4tobi u4esti eio v sleduiu6em periode
            FileIndexWrite( ftFile->ftLastMeasurementValue, currentTariffIndex, &lastValue[ currentTariffIndex ] );
            
            sumToCollect += unitsConsumed * chargePerUnit;
            FileWrite( ftFile->ftSumToCollect, &sumToCollect );
            newCollection = true;            
        }            
    }
}

void PaymentChargeClass::ExecuteTimeBasedCollection()
{
    if( chargeCfg->period != 0 )
    {
        u32 periodCounter = CalcPeriodPassed();  /* Getting how much periods has past since last_collection_time */
        if( periodCounter > 0 )
        {
            sumToCollect += chargeCfg->unitChargeActive.chargeTableElement[0].chargePerUnit * periodCounter;
            FileWrite( ftFile->ftSumToCollect, &sumToCollect );                    
            newCollection = true;
        }
    }
    
    return;
}

void PaymentChargeClass::ExecutePaymentEventBasedCollection( s32 topUpSum )
{
    if( isLinkedAccountActive )
    {
        if( chargeCfg->chargeType == PaymentChargePaymentEventBased )
        {
            if( chargeCfg->chargeConfiguration & chargePercentageBaseCollection )           
            {
                sumToCollect += (topUpSum * chargeCfg->proportion) / 10000;
                FileWrite( ftFile->ftSumToCollect, &sumToCollect );
                newCollection = true;
            }
            else
            {
                sumToCollect += chargeCfg->unitChargeActive.chargeTableElement[0].chargePerUnit;
                FileWrite( ftFile->ftSumToCollect, &sumToCollect );
                newCollection = true;
            }
        }
    }
    return;
}


void PaymentChargeClass::IdleSecond()
{
    if( isLinkedAccountActive )
    {
        /* Described in Blue Book. Charge - period */
        if( chargeCfg->chargeType == PaymentChargeConsumptionBased )
        {   
            ExecuteConsumptionBasedCollection();         
        }
        else if( chargeCfg->chargeType == PaymentChargeTimeBased )
        {
            ExecuteTimeBasedCollection();
        }   

        if( newCollection )                                              
        {
            if( !(chargeCfg->chargeConfiguration & chargeContinuousCollection) )
            {
                if( currValues.totalAmountRemaining != 0 )              // if totalAmountRemaining == 0 then this functionality doesn't work
                {
                    sumToCollect = ReduceTotalAmountRemaining( sumToCollect );         // Vozmijno umeni6itsea sumToCollect
                    FileWrite( ftFile->ftSumToCollect, &sumToCollect );
//                    if( currValues.totalAmountRemaining == 0 )
//                    {
//                        // надо отключить сборы с этого charge потомучто лимит исчерпан
//                    }
                }
            }     
        }
    }

    return;
}

void PaymentChargeClass::IdleMinute()
{
    if( !cmpDateTimeWithNotSpecifiedDateTime( &chargeCfg->unitChargeActivationTime ) )
    {
        if( cmpDateTimeWithCurrDateTime( &chargeCfg->unitChargeActivationTime ) )
            ActivatePassiveUnitCharge();
    }
}

void PaymentChargeClass::Init()
{
    FileRead( ftFile->ftTotalAmountPaid, &currValues.totalAmountPaid );
    
    const u8 lenBufUnitCharge = eDTL_Integer + eDTL_Integer + eDTL_LongUnsigned + 6 + eDTL_Integer + MAX_TARIFFS * ( MAX_INDEX_LEN + eDTL_Long );
    BYTE bufUnitCharge[ lenBufUnitCharge ] = {};
    
    bool needActivatePassiveUnitCharge = true;
    if( FileRead( ftFile->ftUnitChargeActive, bufUnitCharge ) == lenBufUnitCharge )     /* if in the flash was written unit_charge_active */
    {
        convertUnitChargeFromMemoryBufferToStruct( bufUnitCharge, &chargeCfg->unitChargeActive );
        needActivatePassiveUnitCharge = false;                                          /* because unit_charge_active was saved we don't need to activate unit_charge_passive */
    }    
    
    if( FileRead( ftFile->ftUnitChargePassive, bufUnitCharge ) == lenBufUnitCharge )    /* if in the flash was written unit_charge_passive */
    {
        convertUnitChargeFromMemoryBufferToStruct( bufUnitCharge, &chargeCfg->unitChargePassive );
    }
    
    DWORD seconds = 0;
    if( FileRead( ftFile->ftUnitChargeActivationTime, &seconds ) == eDTL_DoubleLongUnsigned ) /* if in the flash was written unit_charge_activation_time */
    {        
        UnpackTDateTime( seconds, &chargeCfg->unitChargeActivationTime );
    }
    else
    {
        chargeCfg->unitChargeActivationTime = { {0xff, 0xff, 0xff, 0xff, 0xff}, {0xff, 0xff, 0xff, 0xff}, 0xff, 0xff, {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff} };
    }
    
    u32 tmpPeriod = 0;
    if( FileRead( ftFile->ftPeriod, &tmpPeriod ) != 0 )           /* read period from the flash. If nothing was written there, then will be used period from default cfg struct */
    {
        chargeCfg->period = tmpPeriod;
    }
      
    seconds = 0;
    if( FileRead( ftFile->ftLastCollectionTime, &seconds ) == eDTL_DoubleLongUnsigned )    /* if in the FRAM was written last_collection_time */
    {
        UnpackTDateTime( seconds, &currValues.lastCollectionTime );
    }
    else
    {
        currValues.lastCollectionTime = { {0xff, 0xff, 0xff, 0xff, 0xff}, {0xff, 0xff, 0xff, 0xff}, 0xff, 0xff, {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff} };
    }
    
    FileRead( ftFile->ftLastCollectionAmount, &currValues.lastCollectionAmount );

    FileRead( ftFile->ftTotalAmountRemaining, &currValues.totalAmountRemaining );
    
    if( chargeCfg->chargeType == PaymentChargeConsumptionBased )
    {
        for( u8 i = 0; i < MAX_TARIFFS; ++i )
        {
            FileIndexRead( ftFile->ftLastMeasurementValue, i, &lastValue[i] );
        }
    }
    
    FileRead( ftFile->ftSumToCollect, &sumToCollect );
    if( sumToCollect != 0 )
        newCollection = true;
    else
        newCollection = false;
    
    if( needActivatePassiveUnitCharge )
    {
        ActivatePassiveUnitCharge();
    } 
}

PaymentChargeClass::PaymentChargeClass( const LOGICAL_NAME* const _ln,
                                       PaymentChargeCfg* const cfg,
                                       const ftPaymentCharge* const _ftFile) : ln( _ln ), chargeCfg(cfg), ftFile( _ftFile )
{
#ifndef NEW_CONST_CLASS_MAP                                                                           
      //DataObjectsMap[(LOGICAL_NAME*)_ln] = this; 
      PaymentChargeObjectsMap[(LOGICAL_NAME*)ln] = this;  
#endif // NEW_CONST_CLASS_MAP
}

/*********************************************/
/*** Functions of PaymentTokenGatewayClass ***/
/*********************************************/


/************************************************************************************************/
/*********** GET and SET functions for communication between Payment classes ********************/
/************************************************************************************************/

/*********************************************/
/***** GET of PaymentAccountClass *************/
/*********************************************/

bool PaymentAccountClass::IsAccountActive() const
{
    return ( (accountCfg->modeAndStatus.accountStatus == activeAccount) ? true : false );
}  

//eT_tokenStatusCode PaymentAccountClass::RedirectToTokenGatewayEnter( uint8_t* buf_request ) const
//{
//    return tokenGateway->Enter( buf_request );
//}

s32 PaymentAccountClass::GetSumOfAllCurrentCreditAmount() const
{
    s32 sumOfAllCurrentCreditAmount = 0;
    
    for( u8 i = 0; i < lenCreditList; ++i )
    {
        sumOfAllCurrentCreditAmount += creditList[i]->GetCurrentCreditAmount();
    }
    
    return sumOfAllCurrentCreditAmount;
} 

TDateTime* PaymentAccountClass::GetAccountActivationTime() const
{
    return &accountCfg->accountActivationTime;
}

TDateTime* PaymentAccountClass::GetAccountClosureTime() const
{
    return &accountCfg->accountClosureTime;
}

s8 PaymentAccountClass::GetCurrencyScale() const
{
    return accountCfg->currency.scale;
}

s32 PaymentAccountClass::GetSumOfAllChargeTotalAmountPaid() const
{
    s32 sumOfAllChargeTotalAmountPaid = 0;
    
    for( u8 i = 0; i < lenChargeList; ++i )
    {
        s32 tmpTotalAmountPaid = chargeList[i]->GetTotalAmountPaid();        
        s8 scaleOfTmpTotalAmountPaid = chargeList[i]->GetPriceScale();
        
        s8 commonScaler = scaleOfTmpTotalAmountPaid - accountCfg->currency.scale;
        
        sumOfAllChargeTotalAmountPaid += scaleValue( tmpTotalAmountPaid, commonScaler );
    }
    
    return sumOfAllChargeTotalAmountPaid;
}

/*********************************************/
/***** GET of PaymentCreditClass *************/
/*********************************************/

const LOGICAL_NAME* PaymentCreditClass::GetLn() const
{
    return ln;
}

s32 PaymentCreditClass::GetCurrentCreditAmount() const
{
    return currValues.currentCreditAmount;
}

eT_creditType PaymentCreditClass::GetCreditType() const
{
    return creditCfg->creditType;
}

u8 PaymentCreditClass::GetPriority() const
{
    return creditCfg->priority;
}

s32 PaymentCreditClass::GetWarningThreshold() const
{
    return creditCfg->warningThreshold;
}

eT_creditStatus PaymentCreditClass::GetCreditStatus() const
{
    return currValues.creditStatus;
}

s32 PaymentCreditClass::GetPresetCreditAmount() const
{
    return creditCfg->presetCreditAmount;
}

s32 PaymentCreditClass::GetCreditAvailableThreshold() const
{
    return creditCfg->creditAvailableThreshold;
}

bool PaymentCreditClass::GetRequiresCreditAmountToBePaidBack() const
{
    return creditCfg->creditConfiguration & creditCfgRepayment;
}

/*********************************************/
/***** SET of PaymentCreditClass *************/
/*********************************************/

bool PaymentCreditClass::InvokeCreditStatusToInUse()
{    
    if( currValues.creditStatus == SELECTED ||
        currValues.creditStatus == IN_USE ||
        ( currValues.creditStatus == ENABLED && !(creditCfg->creditConfiguration & creditCfgConfirmation) ) )
    {
        currValues.creditStatus = IN_USE;
        FileWrite( ftFile->ftCreditStatus, &currValues.creditStatus );
        
        return true;
    }
    else if( currValues.creditStatus == ENABLED && (creditCfg->creditConfiguration & creditCfgConfirmation) )
    {
        currValues.creditStatus = SELECTABLE;
        FileWrite( ftFile->ftCreditStatus, &currValues.creditStatus );
    }
    
    return false;
}

bool PaymentCreditClass::InvokeCreditStatusToEnable()
{
    if( currValues.creditStatus == SELECTABLE ||
        currValues.creditStatus == SELECTED ||
        currValues.creditStatus == IN_USE )
    {
        currValues.creditStatus = ENABLED; 
        FileWrite( ftFile->ftCreditStatus, &currValues.creditStatus );
        
        return true;
    }
    
    return false;
}

void PaymentCreditClass::ResetCredit()
{
    currValues.currentCreditAmount = 0;
    currValues.creditStatus = EXHAUSTED;
    
    FileWrite( ftFile->ftCurrentCreditAmount, &currValues.currentCreditAmount );
    FileWrite( ftFile->ftCreditStatus, &currValues.creditStatus );
}

/*********************************************/
/***** GET of PaymentChargeClass *************/
/*********************************************/

const LOGICAL_NAME* PaymentChargeClass::GetLn() const
{
    return ln;
}

s32 PaymentChargeClass::GetTotalAmountPaid() const
{
    return currValues.totalAmountPaid;
}

u8 PaymentChargeClass::GetPriority() const
{
    return chargeCfg->priority;
}

s8 PaymentChargeClass::GetPriceScale() const
{
    return chargeCfg->unitChargeActive.chargePerUnitScaling.priceScale;
}

bool PaymentChargeClass::GetContinuousCollection() const
{
    return chargeCfg->chargeConfiguration & chargeContinuousCollection;
}

s32 PaymentChargeClass::GetTotalAmountRemaining() const
{
    return currValues.totalAmountRemaining;
}

bool PaymentChargeClass::GetNewCollection() const
{
    return newCollection;
}

s32 PaymentChargeClass::GetSumToCollect() const
{
    return sumToCollect;
}

/*********************************************/
/***** SET of PaymentChargeClass *************/
/*********************************************/
void PaymentChargeClass::ConfirmCollection()
{
    UpdateTotalAmountPaid( sumToCollect );
    UpdateLastCollectionTime();
    UpdateLastCollectionAmount( sumToCollect );   
  
    sumToCollect = 0;
    FileWrite( ftFile->ftSumToCollect, &sumToCollect );
    newCollection = false;
}

void PaymentChargeClass::RefuseCollection()
{
//    UpdateTotalAmountPaid( sumToCollect );    /* t.k. v slu4ae otkaza sumToCollect kajdii raz uveli4ivaetsea, to mi budem po neskoliko raz uveli4ivati total_amount_paid - nepravelino */
//    UpdateLastCollectionTime();
//    UpdateLastCollectionAmount( sumToCollect );   
}

void PaymentChargeClass::ActivateCharge()
{
    isLinkedAccountActive = true;
    
    ActivatePassiveUnitCharge();
    
    UpdateLastCollectionTime();       // The activation time will be the starting point for the consumption based and time based collections
    
    for( u8 i = 0; i < MAX_TARIFFS; ++i )
    {
        /* !!! Nado 4itati iz tariffnogo registra !!! */            
        lastValue[i] = GetValueFromRegister( &chargeCfg->unitChargeActive.commodityReference.logicalName );
        
        FileIndexWrite( ftFile->ftLastMeasurementValue, i, &lastValue[i] );           
    }
}

void PaymentChargeClass::CloseCharge()
{
    isLinkedAccountActive = false;
}

void PaymentChargeClass::ResetCharge()
{
    currValues.totalAmountPaid = 0;
    currValues.lastCollectionTime = { {0xff, 0xff, 0xff, 0xff, 0xff}, {0xff, 0xff, 0xff, 0xff}, 0xff, 0xff, {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff} };
    currValues.lastCollectionAmount = 0;
    currValues.totalAmountRemaining = 0;
    newCollection = false;
    sumToCollect = 0;
    
    FileWrite( ftFile->ftTotalAmountPaid, &currValues.totalAmountPaid );
    u32 seconds = PackTdateTime( &currValues.lastCollectionTime );
    FileWrite( ftFile->ftLastCollectionTime, &seconds );
    FileWrite( ftFile->ftLastCollectionAmount, &currValues.lastCollectionAmount );
    FileWrite( ftFile->ftTotalAmountRemaining, &currValues.totalAmountRemaining );
    FileWrite( ftFile->ftSumToCollect, &sumToCollect );
}

void PaymentChargeClass::SetIsLinkedAccountActive( bool newIsLinkedAccountActive )
{
    isLinkedAccountActive = newIsLinkedAccountActive;
}

/*********************************************/
/***** GET of PaymentTokenGatewayClass *******/
/*********************************************/

bool PaymentTokenGatewayClass::GetPermissionToReconnectRelay() const
{
    if( lastToken.subtype == inTokenSubtype::stopPaidToken ||
        lastToken.subtype == inTokenSubtype::stopNonPaidToken )
    {
        return false;
    }
    return true;
}

//bool PaymentTokenGatewayClass::GetNewTokenTopUp() const
//{
//    return newTokenTopUp;
//}

//s32 PaymentTokenGatewayClass::GetTopUpSum() const
//{
//    return topUpSum;
//}

u32 PaymentTokenGatewayClass::GetTokenID() const
{
    return lastToken.tokenID;
}

const BYTE* PaymentTokenGatewayClass::GetActiveTransactionID() const
{
    return lastToken.activeTransactionID;
}

u32 PaymentTokenGatewayClass::GetExpiresTimeSec() const
{
    return lastToken.expiresTimeSecReceivedWithStart;
}

u8 PaymentTokenGatewayClass::GetExpiresTimeStatus() const
{
    return lastToken.expiresTimeStatusReceivedWithStart;
}

/*********************************************/
/***** SET of PaymentTokenGatewayClass *******/
/*********************************************/

void PaymentTokenGatewayClass::ConfirmReceivedToken()
{
//    newTokenTopUp = false;
//    topUpSum = 0;
    currValues.tokenStatus.statusCode = executionOK;
    FileWrite( ftFile->ftTokenStatusCode, &currValues.tokenStatus.statusCode );
}

void PaymentTokenGatewayClass::RefuseReceivedToken()
{
//    newTokenTopUp = false;
    currValues.tokenStatus.statusCode = executionFAIL;
    FileWrite( ftFile->ftTokenStatusCode, &currValues.tokenStatus.statusCode );
}

/************************************************************************************************/
/******************* GET and SET functions of Payment classes ***********************************/
/************************************************************************************************/

/*********************************************/
/***** GET and SET of PaymentAccountClass ****/
/*********************************************/

bool PaymentAccountClass::GetAttr2( uint8_t* buf_response, uint16_t& len_response ) const
{
    buf_response[len_response++] = Structure;
    buf_response[len_response++] = 2;
    buf_response[len_response++] = Enum;
    buf_response[len_response++] = accountCfg->modeAndStatus.paymentMode;
    buf_response[len_response++] = Enum;
    buf_response[len_response++] = accountCfg->modeAndStatus.accountStatus;
    
    return true;
}

bool PaymentAccountClass::GetAttr3( uint8_t* buf_response, uint16_t& len_response ) const
{
    buf_response[len_response++] = Unsigned;
    buf_response[len_response++] = currValues.currCreditInUse;
    
    return true;
}

bool PaymentAccountClass::GetAttr4( uint8_t* buf_response, uint16_t& len_response ) const
{
    AXDREncodeBitStringTag( &buf_response[len_response], (u8*)&currValues.currCreditStatus, 8 );
    len_response += 3;    
    
    return true;
}

bool PaymentAccountClass::GetAttr5( uint8_t* buf_response, uint16_t& len_response ) const
{
    buf_response[len_response++] = DoubleLong;
    AXDREncodeDoubleLong( &buf_response[len_response], currValues.availableCredit );
    len_response += eDTL_DoubleLong;
    
    return true;
}

bool PaymentAccountClass::GetAttr6( uint8_t* buf_response, uint16_t& len_response ) const
{
    buf_response[len_response++] = DoubleLong;
    AXDREncodeDoubleLong( &buf_response[len_response], currValues.amountToClear );
    len_response += eDTL_DoubleLong;
    
    return true;
}

bool PaymentAccountClass::GetAttr7( uint8_t* buf_response, uint16_t& len_response ) const
{
    buf_response[len_response++] = DoubleLong;
    AXDREncodeDoubleLong( &buf_response[len_response], accountCfg->clearanceThreshold );
    len_response += eDTL_DoubleLong;
    
    return true;
}

bool PaymentAccountClass::GetAttr8( uint8_t* buf_response, uint16_t& len_response ) const
{
    buf_response[len_response++] = DoubleLong;
    AXDREncodeDoubleLong( &buf_response[len_response], currValues.aggregatedDebt );
    len_response += eDTL_DoubleLong;
    
    return true;    
}

bool PaymentAccountClass::GetAttr9( uint8_t* buf_response, uint16_t& len_response ) const
{
    buf_response[len_response++] = Array;
    buf_response[len_response++] = lenCreditList;
    
    for( uint8_t i = 0; i < lenCreditList; ++i )
    {
        AXDREncodeOctetStringTag( &buf_response[len_response], (u8*)accountCfg->creditRefList[i], 6 );
        len_response += 8;
    }
    
    return true;
}

bool PaymentAccountClass::GetAttr10( uint8_t* buf_response, uint16_t& len_response ) const
{
    buf_response[len_response++] = Array;
    buf_response[len_response++] = lenChargeList;
    
    for( uint8_t i = 0; i < lenChargeList; ++i )
    {
        AXDREncodeOctetStringTag( &buf_response[len_response], (u8*)accountCfg->chargeRefList[i], 6 );
        len_response += 8;
    }
    
    return true;
}

bool PaymentAccountClass::GetAttr11( uint8_t* buf_response, uint16_t& len_response ) const
{
    buf_response[len_response++] = Array;
    buf_response[len_response++] = lenCreditChargeCfgList;  
    
    if( lenCreditChargeCfgList == 0 )
        return true;
    
    /* Write all structures */
    for( uint8_t i = 0; i < lenCreditChargeCfgList; ++i )
    {
        buf_response[len_response++] = Structure;
        buf_response[len_response++] = 3; 
        
        AXDREncodeOctetStringTag( &buf_response[len_response], (u8*)accountCfg->creditChargeCfg[i].creditRef, 6 );
        len_response += 8;
        
        AXDREncodeOctetStringTag( &buf_response[len_response], (u8*)accountCfg->creditChargeCfg[i].chargeRef, 6 );
        len_response += 8;
        
        AXDREncodeBitStringTag( &buf_response[len_response], &accountCfg->creditChargeCfg[i].collectionCfg, 8 );
        len_response += 3;
    }
  
    return true;
}

bool PaymentAccountClass::GetAttr12( uint8_t* buf_response, uint16_t& len_response ) const
{
    buf_response[len_response++] = Array;
    buf_response[len_response++] = lenTokenGatewayCfgList;
    
    if( lenTokenGatewayCfgList == 0 )
        return true;
    
    /* Write all structures */
    for( u8 i = 0; i < lenTokenGatewayCfgList; ++i )
    {
        buf_response[len_response++] = Structure;
        buf_response[len_response++] = 2; 
        
        AXDREncodeOctetStringTag( &buf_response[len_response], (u8*)&accountCfg->tokenGatewayCfg[i].creditRef, 6 );
        len_response += 8;
                    
        buf_response[len_response++] = Unsigned;
        buf_response[len_response++] = accountCfg->tokenGatewayCfg[i].tokenProportion;    
    }
    
    return true;
}  

bool PaymentAccountClass::GetAttr13( uint8_t* buf_response, uint16_t& len_response ) const
{
    buf_response[len_response++] = eDT_OctetString;
    buf_response[len_response++] = eDTL_DateTime;
    memcpy( &buf_response[len_response], &accountCfg->accountActivationTime, eDTL_DateTime );
    len_response += eDTL_DateTime;
    
    return true;
}

bool PaymentAccountClass::GetAttr14( uint8_t* buf_response, uint16_t& len_response ) const
{
    buf_response[len_response++] = eDT_OctetString;
    buf_response[len_response++] = eDTL_DateTime;
    memcpy( &buf_response[len_response], &accountCfg->accountClosureTime, eDTL_DateTime );
    len_response += eDTL_DateTime;
    
    return true;
}

bool PaymentAccountClass::GetAttr15( uint8_t* buf_response, uint16_t& len_response ) const
{
    buf_response[len_response++] = Structure;
    buf_response[len_response++] = 3;    
    
    /* Find out len of currency name */
    u8 lenName = 0;
    while( accountCfg->currency.name[lenName] != 0 )
    {
        lenName++;
        if( lenName == MAX_LEN_CURRENCY_NAME )
            break;
    }
    
    buf_response[len_response++] = Utf8String;
    buf_response[len_response++] = lenName;
    
    for( u8 i = 0; i < lenName; ++i )
    {
        buf_response[len_response++] = accountCfg->currency.name[i];
    }
    
    buf_response[len_response++] = Integer;
    buf_response[len_response++] = accountCfg->currency.scale;

    buf_response[len_response++] = Enum;
    buf_response[len_response++] = accountCfg->currency.unit;
    
    return true;
}

bool PaymentAccountClass::GetAttr16( uint8_t* buf_response, uint16_t& len_response ) const
{
    buf_response[len_response++] = DoubleLong;
    AXDREncodeDoubleLong( &buf_response[len_response], currValues.lowCreditThreshold );
    len_response += eDTL_DoubleLong;

    return true;
}

bool PaymentAccountClass::GetAttr17( uint8_t* buf_response, uint16_t& len_response ) const
{
    buf_response[len_response++] = DoubleLong;
    AXDREncodeDoubleLong( &buf_response[len_response], currValues.nextCreditAvailableThreshold );
    len_response += eDTL_DoubleLong;

    return true;
}

bool PaymentAccountClass::GetAttr18( uint8_t* buf_response, uint16_t& len_response ) const
{
    buf_response[len_response++] = LongUnsigned;
    AXDREncodeWord( &buf_response[len_response], accountCfg->maxProvision );
    len_response += eDTL_LongUnsigned;
    
    return true;
}

bool PaymentAccountClass::GetAttr19( uint8_t* buf_response, uint16_t& len_response ) const
{
    buf_response[len_response++] = DoubleLong;
    AXDREncodeDoubleLong( &buf_response[len_response], accountCfg->maxProvisionPeriod );
    len_response += eDTL_DoubleLong;

    return true;
}

bool PaymentAccountClass::Get( uint8_t attrID, uint8_t* buf_request, uint8_t* buf_response, uint16_t& len_response )
{
    len_response = 0;
    switch( attrID )
    {
        case PaymentAccountModeAndStatusAttr:               return GetAttr2( buf_response, len_response );
        case PaymentAccountCurrentCreditInUseAttr:          return GetAttr3( buf_response, len_response );
        case PaymentAccountCurrentCreditStatusAttr:         return GetAttr4( buf_response, len_response );
        case PaymentAccountAvailableCreditAttr:             return GetAttr5( buf_response, len_response );
        case PaymentAccountAmountToClearAttr:               return GetAttr6( buf_response, len_response );
        case PaymentAccountClearanceThresholdAttr:          return GetAttr7( buf_response, len_response );
        case PaymentAccountAggregatedDebtAttr:              return GetAttr8( buf_response, len_response );
        case PaymentAccountCreditReferenceListAttr:         return GetAttr9( buf_response, len_response );
        case PaymentAccountChargeReferenceListAttr:         return GetAttr10( buf_response, len_response );
        case PaymentAccountCreditChargeConfigurationAttr:   return GetAttr11( buf_response, len_response );
        case PaymentAccountTokenGatewayConfigurationAttr:   return GetAttr12( buf_response, len_response );
        case PaymentAccountAccountActivationTimeAttr:       return GetAttr13( buf_response, len_response );
        case PaymentAccountAccountClosureTimeAttr:          return GetAttr14( buf_response, len_response );
        case PaymentAccountCurrencyAttr:                    return GetAttr15( buf_response, len_response );
        case PaymentAccountLowCreditThresholdAttr:          return GetAttr16( buf_response, len_response );
        case PaymentAccountNextCreditAvailableThresholdAttr:return GetAttr17( buf_response, len_response );
        case PaymentAccountMaxProvisionAttr:                return GetAttr18( buf_response, len_response );
        case PaymentAccountMaxProvisionPeriodAttr:          return GetAttr19( buf_response, len_response );
        default:
          *buf_response = ObjectUndefined;
          return false;
    }
}

uint8_t PaymentAccountClass::SetAttr7( uint8_t* buf_request )
{
    if( buf_request[0] != DoubleLong )
        return TypeUnmatched;
    
    AXDRDecodeDoubleLong( &buf_request[1], &accountCfg->clearanceThreshold );
    
    return eDAR_Success;
}

uint8_t PaymentAccountClass::SetAttr12( uint8_t* buf_request )
{
    /* Check all tags and lengths */
    if( buf_request[0] != Array )
        return TypeUnmatched;
    
    if( buf_request[1] > MAX_OBJECTS_IN_TOKEN_GATEWAY_CFG )
        return ReadWriteDenied;
    
    uint8_t pos_in_buf_request = 2;
    for( u8 i = 0; i < buf_request[1]; ++i )
    {        
        if( buf_request[ pos_in_buf_request++ ] != Structure )
            return TypeUnmatched;
        
        if( buf_request[ pos_in_buf_request++ ] != 2 )
            return ReadWriteDenied;
        
        if( buf_request[ pos_in_buf_request++ ] != OctetString )
            return TypeUnmatched;
        
        if( buf_request[ pos_in_buf_request++ ] != 6 )
            return ReadWriteDenied;
        
        if( !findLnInArrayOfLn( accountCfg->creditRefList, (LOGICAL_NAME*)&buf_request[pos_in_buf_request], lenCreditList ) )       // check if there is in creditRefList received LN 
            return ReadWriteDenied;
        
        pos_in_buf_request += 6;
        
        if( buf_request[ pos_in_buf_request++ ] != Unsigned )
            return TypeUnmatched;        
        
        u8 tmpTokenProportion = buf_request[ pos_in_buf_request++ ];
        if( tmpTokenProportion > 100 )
            return ReadWriteDenied;        
    }
    
    /* Clear all previous configuration */
    memset( accountCfg->tokenGatewayCfg, 0, sizeof(accountCfg->tokenGatewayCfg) );
    
    /* Save all new configuration */
    pos_in_buf_request = 0 + eDTL_Array + eDTL_Unsigned;
    for( u8 i = 0; i < buf_request[1]; ++i )
    {
        pos_in_buf_request += eDTL_Structure + eDTL_Unsigned + eDTL_OctetString + eDTL_Unsigned;
        memcpy( (BYTE*)&accountCfg->tokenGatewayCfg[i].creditRef, &buf_request[ pos_in_buf_request ], 6 );
        
        pos_in_buf_request += 6 + eDTL_Unsigned;
        accountCfg->tokenGatewayCfg[i].tokenProportion = buf_request[ pos_in_buf_request ];
    }
    
    lenTokenGatewayCfgList = buf_request[1];
    
    return eDAR_Success;
}

uint8_t PaymentAccountClass::SetAttr13( uint8_t* buf_request )
{
    /* Check all tags and lengths */
    uint8_t pos_in_buf_request = 0;
    if( buf_request[pos_in_buf_request] != OctetString && buf_request[pos_in_buf_request] != DateTime  )
        return eDAR_TypeUnmatched;
    
    if( buf_request[pos_in_buf_request++] == OctetString )
    {
        if( buf_request[ pos_in_buf_request++ ] != eDTL_DateTime )
            return eDAR_TypeUnmatched;
    }
    
    BYTE bufDateTime[eDTL_DateTime] = {};
    memcpy( bufDateTime, &buf_request[ pos_in_buf_request ], eDTL_DateTime );
    
    if( bufDateTime[0] == 0xff &&
        bufDateTime[1] == 0xff &&
        bufDateTime[2] == 0xff &&
        bufDateTime[3] == 0xff &&
        bufDateTime[4] == 0xff &&
        bufDateTime[5] == 0xff &&
        bufDateTime[6] == 0xff &&
        bufDateTime[7] == 0xff &&
        bufDateTime[8] == 0xff &&
        bufDateTime[9] == 0xff &&
        bufDateTime[10] == 0xff &&
        bufDateTime[11] == 0xff )
    {
        /* Clear previous DateTime */
        memset( &accountCfg->accountActivationTime, 0, sizeof( accountCfg->accountActivationTime ) );
        
        /* Save all 0xff */
        memcpy( &accountCfg->accountActivationTime, bufDateTime, sizeof( bufDateTime ) ); 
        
        return eDAR_Success;
    }
    
    TDateTime tmpStructDateTime;
    memcpy( &tmpStructDateTime, bufDateTime, sizeof( tmpStructDateTime ) );
    
    if( !IsValidWildcard( &tmpStructDateTime ) ||
        (tmpStructDateTime.date.year_hi == 0xff && tmpStructDateTime.date.year_low == 0xff) ||
        tmpStructDateTime.date.month == 0xff ||
        tmpStructDateTime.date.day == 0xff ||
        tmpStructDateTime.time.hour == 0xff ||
        tmpStructDateTime.time.minute == 0xff ||
        tmpStructDateTime.time.second == 0xff ||
        tmpStructDateTime.time.hundredths == 0xff )
        return eDAR_OtherReason;
    
    /* Clear previous DateTime */
    memset( &accountCfg->accountActivationTime, 0, sizeof( accountCfg->accountActivationTime ) );
    
    /* Save new DateTime */
    memcpy( &accountCfg->accountActivationTime, &tmpStructDateTime, sizeof( tmpStructDateTime ) );
  
    return eDAR_Success;
}

uint8_t PaymentAccountClass::SetAttr14( uint8_t* buf_request )
{
    /* Check all tags and lengths */
    uint8_t pos_in_buf_request = 0;
    if( buf_request[pos_in_buf_request] != OctetString && buf_request[pos_in_buf_request] != DateTime  )
        return eDAR_TypeUnmatched;
    
    if( buf_request[pos_in_buf_request++] == OctetString )
    {
        if( buf_request[ pos_in_buf_request++ ] != eDTL_DateTime )
            return eDAR_TypeUnmatched;
    }
    
    BYTE bufDateTime[eDTL_DateTime] = {};
    memcpy( bufDateTime, &buf_request[ pos_in_buf_request ], eDTL_DateTime );
    
    if( bufDateTime[0] == 0xff &&
        bufDateTime[1] == 0xff &&
        bufDateTime[2] == 0xff &&
        bufDateTime[3] == 0xff &&
        bufDateTime[4] == 0xff &&
        bufDateTime[5] == 0xff &&
        bufDateTime[6] == 0xff &&
        bufDateTime[7] == 0xff &&
        bufDateTime[8] == 0xff &&
        bufDateTime[9] == 0xff &&
        bufDateTime[10] == 0xff &&
        bufDateTime[11] == 0xff )
    {
        /* Clear previous DateTime */
        memset( &accountCfg->accountClosureTime, 0, sizeof( accountCfg->accountClosureTime ) );
        
        /* Save all 0xff */
        memcpy( &accountCfg->accountClosureTime, bufDateTime, sizeof( bufDateTime ) ); 
        
        return eDAR_Success;
    }
    
    TDateTime tmpStructDateTime;
    memcpy( &tmpStructDateTime, bufDateTime, sizeof( tmpStructDateTime ) );
    
    if( !IsValidWildcard( &tmpStructDateTime ) ||
        (tmpStructDateTime.date.year_hi == 0xff && tmpStructDateTime.date.year_low == 0xff) ||
        tmpStructDateTime.date.month == 0xff ||
        tmpStructDateTime.date.day == 0xff ||
        tmpStructDateTime.time.hour == 0xff ||
        tmpStructDateTime.time.minute == 0xff ||
        tmpStructDateTime.time.second == 0xff ||
        tmpStructDateTime.time.hundredths == 0xff )
        return eDAR_OtherReason;
    
    /* Clear previous DateTime */
    memset( &accountCfg->accountClosureTime, 0, sizeof( accountCfg->accountClosureTime ) );
    
    /* Save new DateTime */
    memcpy( &accountCfg->accountClosureTime, &tmpStructDateTime, sizeof( tmpStructDateTime ) );
  
    return eDAR_Success;
}

uint8_t PaymentAccountClass::SetAttr15( uint8_t* buf_request )
{
    /* Check all tags and lengths */
    uint8_t pos_in_buf_request = 0;
    if( buf_request[ pos_in_buf_request++ ] != Structure )
        return eDAR_TypeUnmatched;
    
    if( buf_request[ pos_in_buf_request++ ] != 3 )
        return eDAR_OtherReason;
    
    if( buf_request[ pos_in_buf_request++ ] != Utf8String )
        return eDAR_TypeUnmatched;
    
    u8 lenCurrencyName = buf_request[ pos_in_buf_request ];
    if( lenCurrencyName > MAX_LEN_CURRENCY_NAME || lenCurrencyName == 0 )
        return eDAR_OtherReason;
    
    pos_in_buf_request += eDTL_Integer;
    pos_in_buf_request += lenCurrencyName;
    
    if( buf_request[ pos_in_buf_request++ ] != Integer )
        return eDAR_TypeUnmatched;
    
    pos_in_buf_request += eDTL_Integer;
    
    if( buf_request[ pos_in_buf_request++ ] != Enum )
        return eDAR_TypeUnmatched;
    
    /* Clear previous currency_name */
    memset( &accountCfg->currency.name, 0, sizeof( accountCfg->currency.name ) );
    
    /* Save new currency_name */
    pos_in_buf_request = 4;
    memcpy( &accountCfg->currency.name, &buf_request[pos_in_buf_request], lenCurrencyName );
    
    pos_in_buf_request += lenCurrencyName + eDTL_Integer;
    accountCfg->currency.scale = buf_request[ pos_in_buf_request ];
    pos_in_buf_request += eDTL_Enum + eDTL_Integer;
    accountCfg->currency.unit = static_cast<eT_currency_unit>( buf_request[ pos_in_buf_request ] );   
    
    u8 bufForFlash[MAX_LEN_CURRENCY_NAME + eDTL_Integer + eDTL_Enum] = {};
    memcpy( &bufForFlash[0], &accountCfg->currency.name, MAX_LEN_CURRENCY_NAME);
    bufForFlash[3] = accountCfg->currency.scale;
    bufForFlash[4] = accountCfg->currency.unit;
    
    FileWrite( ftFile->ftCurrency, bufForFlash );    
    
    return eDAR_Success;
}

uint8_t PaymentAccountClass::SetAttr18( uint8_t* buf_request )
{
    if( buf_request[0] != LongUnsigned )
        return eDAR_TypeUnmatched;
  
    AXDRDecodeWord( &buf_request[1], &accountCfg->maxProvision );
    
    FileWrite( ftFile->ftMaxProvision, &accountCfg->maxProvision );
    
    return eDAR_Success;
}

uint8_t PaymentAccountClass::SetAttr19( uint8_t* buf_request )
{
    if( buf_request[0] != DoubleLong )
        return eDAR_TypeUnmatched;
    
    AXDRDecodeDoubleLong( &buf_request[1], &accountCfg->maxProvisionPeriod );
  
    FileWrite( ftFile->ftMaxProvisionPeriod, &accountCfg->maxProvisionPeriod );
    
    return eDAR_Success;
}

uint8_t PaymentAccountClass::Set( uint8_t attrID, uint8_t* buf_request )
{
    switch( attrID )
    {
        case PaymentAccountClearanceThresholdAttr:              return SetAttr7( buf_request );
        case PaymentAccountTokenGatewayConfigurationAttr:       return SetAttr12( buf_request );
        case PaymentAccountAccountActivationTimeAttr:           return SetAttr13( buf_request );
        case PaymentAccountAccountClosureTimeAttr:              return SetAttr14( buf_request );
        case PaymentAccountCurrencyAttr:                        return SetAttr15( buf_request );
        case PaymentAccountMaxProvisionAttr:                    return SetAttr18( buf_request );
        case PaymentAccountMaxProvisionPeriodAttr:              return SetAttr19( buf_request );
        default:                                                return ObjectUndefined;
    }
}

/*********************************************/
/***** GET and SET of PaymentCreditClass *****/
/*********************************************/

bool PaymentCreditClass::GetAttr2( uint8_t* buf_response, uint16_t& len_response ) const                
{
    buf_response[len_response++] = DoubleLong;
    AXDREncodeDoubleLong( &buf_response[len_response], currValues.currentCreditAmount );
    len_response += eDTL_DoubleLong;
    
    return true;
}

bool PaymentCreditClass::GetAttr3( uint8_t* buf_response, uint16_t& len_response ) const
{
    buf_response[len_response++] = Enum;
    buf_response[len_response++] = creditCfg->creditType;

    return true;
}

bool PaymentCreditClass::GetAttr4( uint8_t* buf_response, uint16_t& len_response ) const
{
    buf_response[len_response++] = Unsigned;
    buf_response[len_response++] = creditCfg->priority;

    return true;
}

bool PaymentCreditClass::GetAttr5( uint8_t* buf_response, uint16_t& len_response ) const
{
    buf_response[len_response++] = DoubleLong;
    AXDREncodeDoubleLong( &buf_response[len_response], creditCfg->warningThreshold );
    len_response += eDTL_DoubleLong;

    return true;
}

bool PaymentCreditClass::GetAttr6( uint8_t* buf_response, uint16_t& len_response ) const
{
    buf_response[len_response++] = DoubleLong;
    AXDREncodeDoubleLong( &buf_response[len_response], creditCfg->limit );
    len_response += eDTL_DoubleLong;

    return true;    
}

bool PaymentCreditClass::GetAttr7( uint8_t* buf_response, uint16_t& len_response ) const
{   
    AXDREncodeBitStringTag( &buf_response[len_response], &creditCfg->creditConfiguration, 8 );
    len_response += 3;
    
    return true;
}

bool PaymentCreditClass::GetAttr8( uint8_t* buf_response, uint16_t& len_response ) const
{    
    buf_response[len_response++] = Enum;
    buf_response[len_response++] = currValues.creditStatus;
    
    return true;
}

bool PaymentCreditClass::GetAttr9( uint8_t* buf_response, uint16_t& len_response ) const               
{
    buf_response[len_response++] = DoubleLong;
    AXDREncodeDoubleLong( &buf_response[len_response], creditCfg->presetCreditAmount );
    len_response += eDTL_DoubleLong;

    return true;   
}

bool PaymentCreditClass::GetAttr10( uint8_t* buf_response, uint16_t& len_response ) const               
{
    buf_response[len_response++] = DoubleLong;
    AXDREncodeDoubleLong( &buf_response[len_response], creditCfg->creditAvailableThreshold );
    len_response += eDTL_DoubleLong;

    return true;   
}

bool PaymentCreditClass::GetAttr11( uint8_t* buf_response, uint16_t& len_response ) const               
{
    buf_response[len_response++] = DateTime;
    memcpy( &buf_response[len_response], &creditCfg->period, eDTL_DateTime );
    len_response += eDTL_DateTime;
    
    return true;
}

bool PaymentCreditClass::Get( uint8_t attrID, uint8_t* buf_request, uint8_t* buf_response, uint16_t& len_response )
{
    len_response = 0;
    switch( attrID )
    {
        case PaymentCreditCurrentCreditAmountAttr:              return GetAttr2( buf_response, len_response );
        case PaymentCreditCreditTypeAttr:                       return GetAttr3( buf_response, len_response );
        case PaymentCreditPriorityAttr:                         return GetAttr4( buf_response, len_response );
        case PaymentCreditWarningThresholdAttr:                 return GetAttr5( buf_response, len_response );
        case PaymentCreditLimitAttr:                            return GetAttr6( buf_response, len_response );
        case PaymentCreditCreditConfigurationAttr:              return GetAttr7( buf_response, len_response );
        case PaymentCreditCreditStatusAttr:                     return GetAttr8( buf_response, len_response );
        case PaymentCreditPresetCreditAmountAttr:               return GetAttr9( buf_response, len_response );
        case PaymentCreditCreditAvailableThresholdAttr:         return GetAttr10( buf_response, len_response );      
        case PaymentCreditPeriodAttr:                           return GetAttr11( buf_response, len_response );
        default:                                                return false;
    }
}

uint8_t PaymentCreditClass::SetAttr3( uint8_t* buf_request )
{
    if( buf_request[0] != Enum )
        return eDAR_TypeUnmatched;
    
    creditCfg->creditType = static_cast<eT_creditType>( buf_request[1] );
    
    return eDAR_Success;
    
}

uint8_t PaymentCreditClass::SetAttr4( uint8_t* buf_request )
{
    if( buf_request[0] != Unsigned )
        return eDAR_TypeUnmatched;
    
    creditCfg->priority = buf_request[1];
    
    return eDAR_Success;
}

uint8_t PaymentCreditClass::SetAttr5( uint8_t* buf_request )
{
    if( buf_request[0] != DoubleLong )
        return eDAR_TypeUnmatched;
    
    AXDRDecodeDoubleLong( &buf_request[1], &creditCfg->warningThreshold );
  
    FileWrite( ftFile->ftWarningThreshold, &creditCfg->warningThreshold );
    
    return eDAR_Success;
}

uint8_t PaymentCreditClass::SetAttr6( uint8_t* buf_request )
{
    if( buf_request[0] != DoubleLong )
        return eDAR_TypeUnmatched;
    
    AXDRDecodeDoubleLong( &buf_request[1], &creditCfg->limit );
    
    FileWrite( ftFile->ftLimit, &creditCfg->limit);
    
    return eDAR_Success;
}

uint8_t PaymentCreditClass::SetAttr7( uint8_t* buf_request )
{
    if( buf_request[0] != BitString )
        return eDAR_TypeUnmatched;
    
    u8 lenBitString = buf_request[1];
    if( lenBitString != 8 )
        return eDAR_TypeUnmatched;
    
    AXDRDecodeBitStringFixed( &buf_request[2], &creditCfg->creditConfiguration, lenBitString );
    
    return eDAR_Success;
}

uint8_t PaymentCreditClass::SetAttr9( uint8_t* buf_request )
{
    if( buf_request[0] != DoubleLong )
        return eDAR_TypeUnmatched;
    
    AXDRDecodeDoubleLong( &buf_request[1], &creditCfg->presetCreditAmount );
    
    return eDAR_Success;
}

uint8_t PaymentCreditClass::SetAttr10( uint8_t* buf_request )
{
    if( buf_request[0] != DoubleLong )
        return eDAR_TypeUnmatched;
    
    AXDRDecodeDoubleLong( &buf_request[1], &creditCfg->creditAvailableThreshold );
    
    return eDAR_Success;
}

uint8_t PaymentCreditClass::SetAttr11( uint8_t* buf_request )
{
    /* Check all tags and lengths */
    uint8_t pos_in_buf_request = 0;
    if( buf_request[pos_in_buf_request] != OctetString && buf_request[pos_in_buf_request] != DateTime  )
        return eDAR_TypeUnmatched;
    
    if( buf_request[pos_in_buf_request++] == OctetString )
    {
        if( buf_request[ pos_in_buf_request++ ] != eDTL_DateTime )
            return eDAR_TypeUnmatched;
    }
    
    BYTE bufDateTime[eDTL_DateTime] = {};
    memcpy( bufDateTime, &buf_request[ pos_in_buf_request ], eDTL_DateTime );
    
    if( bufDateTime[0] == 0xff &&
        bufDateTime[1] == 0xff &&
        bufDateTime[2] == 0xff &&
        bufDateTime[3] == 0xff &&
        bufDateTime[4] == 0xff &&
        bufDateTime[5] == 0xff &&
        bufDateTime[6] == 0xff &&
        bufDateTime[7] == 0xff &&
        bufDateTime[8] == 0xff &&
        bufDateTime[9] == 0xff &&
        bufDateTime[10] == 0xff &&
        bufDateTime[11] == 0xff )
    {
        /* Clear previous DateTime */
        memset( &creditCfg->period, 0, sizeof( creditCfg->period ) );
        
        /* Save all 0xff */
        memcpy( &creditCfg->period, bufDateTime, sizeof( bufDateTime ) ); 
        
        return eDAR_Success;
    }
    
    TDateTime tmpStructDateTime;
    memcpy( &tmpStructDateTime, bufDateTime, sizeof( tmpStructDateTime ) );
    
    if( !IsValidWildcard( &tmpStructDateTime ) ||
        (tmpStructDateTime.date.year_hi == 0xff && tmpStructDateTime.date.year_low == 0xff) ||
        tmpStructDateTime.date.month == 0xff ||
        tmpStructDateTime.date.day == 0xff ||
        tmpStructDateTime.time.hour == 0xff ||
        tmpStructDateTime.time.minute == 0xff ||
        tmpStructDateTime.time.second == 0xff ||
        tmpStructDateTime.time.hundredths == 0xff )
        return eDAR_OtherReason;
    
    /* Clear previous DateTime */
    memset( &creditCfg->period, 0, sizeof( creditCfg->period ) );
    
    /* Save new DateTime */
    memcpy( &creditCfg->period, &tmpStructDateTime, sizeof( tmpStructDateTime ) );
  
    return eDAR_Success;
}

uint8_t PaymentCreditClass::Set( uint8_t attrID, uint8_t* buf_request )
{
    switch( attrID )
    {
        case PaymentCreditCreditTypeAttr:                       return SetAttr3( buf_request );
        case PaymentCreditPriorityAttr:                         return SetAttr4( buf_request );
        case PaymentCreditWarningThresholdAttr:                 return SetAttr5( buf_request );
        case PaymentCreditLimitAttr:                            return SetAttr6( buf_request );
        case PaymentCreditCreditConfigurationAttr:              return SetAttr7( buf_request );
        case PaymentCreditPresetCreditAmountAttr:               return SetAttr9( buf_request );
        case PaymentCreditCreditAvailableThresholdAttr:         return SetAttr10( buf_request );
        case PaymentCreditPeriodAttr:                           return SetAttr11( buf_request );
        default:                                                return eDAR_ObjectUndefined;
    }
}

/*********************************************/
/***** GET and SET of PaymentChargeClass *****/
/*********************************************/

bool PaymentChargeClass::GetAttr2( uint8_t* buf_response, uint16_t& len_response ) const
{
    buf_response[len_response++] = DoubleLong;
    AXDREncodeDoubleLong( &buf_response[len_response], currValues.totalAmountPaid );
    len_response += eDTL_DoubleLong;

    return true;
}

bool PaymentChargeClass::GetAttr3( uint8_t* buf_response, uint16_t& len_response ) const
{  
    buf_response[len_response++] = Enum;
    buf_response[len_response++] = chargeCfg->chargeType;

    return true;
}

bool PaymentChargeClass::GetAttr4( uint8_t* buf_response, uint16_t& len_response ) const
{  
    buf_response[len_response++] = Unsigned;
    buf_response[len_response++] = chargeCfg->priority;

    return true;
}

bool GetPaymentChargeUnitCharge( uint8_t* buf_response, uint16_t& len_response, const PaymentChargeUnitCharge* const src )
{
    buf_response[len_response++] = Structure;
    buf_response[len_response++] = 3;    

    buf_response[len_response++] = Structure;
    buf_response[len_response++] = 2;
    buf_response[len_response++] = Integer;
    buf_response[len_response++] = src->chargePerUnitScaling.commodityScale;
    buf_response[len_response++] = Integer;
    buf_response[len_response++] = src->chargePerUnitScaling.priceScale;
    
    buf_response[len_response++] = Structure;
    buf_response[len_response++] = 3;
    buf_response[len_response++] = LongUnsigned;
    AXDREncodeWord( &buf_response[len_response], src->commodityReference.classId );
    len_response += eDTL_LongUnsigned;
    buf_response[len_response++] = OctetString;
    buf_response[len_response++] = 6;
    memcpy( &buf_response[len_response], &src->commodityReference.logicalName, 6 );
    len_response += 6;
    buf_response[len_response++] = Integer;
    buf_response[len_response++] = src->commodityReference.attributeIndex;

    u8 lenChargeTable = lenChargeTableElement( &src->chargeTableElement[0] );
    buf_response[len_response++] = Array;
    buf_response[len_response++] = lenChargeTable;
    
    if( lenChargeTable == 0 )
        return true;
    
    for( u8 i = 0; i < lenChargeTable; ++i )
    {
        buf_response[len_response++] = Structure;
        buf_response[len_response++] = 2;
        
        if( src->chargeTableElement[i].index[0] == 0x00 )       /* If index is empty (always one tariff) */
        {
            buf_response[len_response++] = OctetString;
            buf_response[len_response++] = 0;
        }
        else
        {
            AXDREncodeOctetStringTag( &buf_response[len_response], (u8*)src->chargeTableElement[i].index, MAX_INDEX_LEN );
            len_response += 2 + MAX_INDEX_LEN;
        }
        
        buf_response[len_response++] = Long;
        AXDREncodeShort( &buf_response[len_response], src->chargeTableElement[i].chargePerUnit );
        len_response += eDTL_Long;
    }    
    
    return true;
}

bool PaymentChargeClass::GetAttr5( uint8_t* buf_response, uint16_t& len_response ) const
{  
    return GetPaymentChargeUnitCharge( buf_response, len_response, &chargeCfg->unitChargeActive );
}

bool PaymentChargeClass::GetAttr6( uint8_t* buf_response, uint16_t& len_response ) const
{  
    return GetPaymentChargeUnitCharge( buf_response, len_response, &chargeCfg->unitChargePassive );
}

bool PaymentChargeClass::GetAttr7( uint8_t* buf_response, uint16_t& len_response ) const
{
    buf_response[len_response++] = eDT_OctetString;
    buf_response[len_response++] = eDTL_DateTime;
    memcpy( &buf_response[len_response], &chargeCfg->unitChargeActivationTime, eDTL_DateTime );
    len_response += eDTL_DateTime;
    
    return true;
}

bool PaymentChargeClass::GetAttr8( uint8_t* buf_response, uint16_t& len_response ) const
{
    buf_response[len_response++] = DoubleLongUnsigned;
    AXDREncodeDword( &buf_response[len_response], chargeCfg->period );
    len_response += eDTL_DoubleLongUnsigned;
    
    return true;
}

bool PaymentChargeClass::GetAttr9( uint8_t* buf_response, uint16_t& len_response ) const
{    
    AXDREncodeBitStringTag( &buf_response[len_response], &chargeCfg->chargeConfiguration, 8 );
    len_response += 3;    

    return true;
}

bool PaymentChargeClass::GetAttr10( uint8_t* buf_response, uint16_t& len_response ) const
{
    buf_response[len_response++] = DateTime;
    memcpy( &buf_response[len_response], &currValues.lastCollectionTime, eDTL_DateTime );
    len_response += eDTL_DateTime;
    
    return true;
}

bool PaymentChargeClass::GetAttr11( uint8_t* buf_response, uint16_t& len_response ) const
{
    buf_response[len_response++] = DoubleLong;
    AXDREncodeDword( &buf_response[len_response], currValues.lastCollectionAmount );
    len_response += eDTL_DoubleLong;
    
    return true;
}

bool PaymentChargeClass::GetAttr12( uint8_t* buf_response, uint16_t& len_response ) const
{
    buf_response[len_response++] = DoubleLong;
    AXDREncodeDword( &buf_response[len_response], currValues.totalAmountRemaining );
    len_response += eDTL_DoubleLong;
    
    return true;
}

bool PaymentChargeClass::GetAttr13( uint8_t* buf_response, uint16_t& len_response ) const
{
    buf_response[len_response++] = LongUnsigned;
    AXDREncodeWord( &buf_response[len_response], chargeCfg->proportion );
    len_response += eDTL_LongUnsigned;
    
    return true;
}

bool PaymentChargeClass::Get( uint8_t attrID, uint8_t* buf_request, uint8_t* buf_response, uint16_t& len_response )
{
    len_response = 0;
    switch( attrID )
    {
        case PaymentChargeTotalAmountPaidAttr:                  return GetAttr2( buf_response, len_response );
        case PaymentChargeChargeTypeAttr:                       return GetAttr3( buf_response, len_response );
        case PaymentChargePriorityAttr:                         return GetAttr4( buf_response, len_response );
        case PaymentChargeUnitChargeActiveAttr:                 return GetAttr5( buf_response, len_response );
        case PaymentChargeUnitChargePassiveAttr:                return GetAttr6( buf_response, len_response );
        case PaymentChargeUnitChargeActivationTimeAttr:         return GetAttr7( buf_response, len_response );
        case PaymentChargePeriodAttr:                           return GetAttr8( buf_response, len_response );
        case PaymentChargeChargeConfigurationAttr:              return GetAttr9( buf_response, len_response );
        case PaymentChargeLastCollectionTimeAttr:               return GetAttr10( buf_response, len_response );
        case PaymentChargeLastCollectionAmountAttr:             return GetAttr11( buf_response, len_response );
        case PaymentChargeTotalAmountRemainingAttr:             return GetAttr12( buf_response, len_response );
        case PaymentChargeProportionAttr:                       return GetAttr13( buf_response, len_response );
        default:                                                return false;
    }
}

uint8_t PaymentChargeClass::SetAttr3( uint8_t* buf_request )
{
    if( buf_request[0] != Enum )
        return eDAR_TypeUnmatched;
    
    if( buf_request[1] > PaymentChargePaymentEventBased )
        return eDAR_ReadWriteDenied;
    
    chargeCfg->chargeType = static_cast<eT_PaymentChargeType>( buf_request[1] );
      
    return eDAR_Success;
}

uint8_t PaymentChargeClass::SetAttr4( uint8_t* buf_request )
{
    if( buf_request[0] != Unsigned )
        return eDAR_TypeUnmatched;
    
    chargeCfg->priority = buf_request[1];
    
    return eDAR_Success;
}

uint8_t SetPaymentChargeUnitCharge( uint8_t* buf_request, PaymentChargeUnitCharge* dst )
{
    /* Check all tags and lengths */
    if( buf_request[0] != Structure )
        return TypeUnmatched;
    
    if( buf_request[1] != 3 )
        return TypeUnmatched;
    
    if( buf_request[2] != Structure )
        return TypeUnmatched;
    
    if( buf_request[3] != 2 )
        return TypeUnmatched;
    
    if( buf_request[4] != Integer )
        return TypeUnmatched;
    
    if( buf_request[6] != Integer )
        return TypeUnmatched;

    if( buf_request[8] != Structure )
        return TypeUnmatched;

    if( buf_request[9] != 3 )
        return TypeUnmatched;
    
    if( buf_request[10] != LongUnsigned )
        return TypeUnmatched;
    
    if( buf_request[13] != OctetString )
        return TypeUnmatched;
    
    if( buf_request[14] != 6 )
        return TypeUnmatched;

    if( buf_request[21] != Integer )
        return TypeUnmatched;
    
    if( buf_request[23] != Array )
        return TypeUnmatched;
    
    u8 lenArray = buf_request[24];
    if( lenArray > MAX_TARIFFS )
        return TypeUnmatched;
    
    u8 pos_in_buf_request = 25;
    for( u8 i = 0; i < lenArray; ++i )
    {
        if( buf_request[pos_in_buf_request++] != Structure )
            return TypeUnmatched;

        if( buf_request[pos_in_buf_request++] != 2 )
            return TypeUnmatched;

        if( buf_request[pos_in_buf_request++] != OctetString )
            return TypeUnmatched;
        
        uint8_t lenOctetString = buf_request[pos_in_buf_request++];
        if( lenOctetString > MAX_INDEX_LEN )
            return TypeUnmatched;
        
        if( lenOctetString == 0 && lenArray > 1 )     /* 09 00 (always one price)- permited only if it is the single in the array */
            return TypeUnmatched;
        
        pos_in_buf_request += lenOctetString;        
        
        if( buf_request[pos_in_buf_request++] != Long )
            return TypeUnmatched;
        
        pos_in_buf_request += eDTL_Long;
    }
    
    /* Clear all previous configuration */
    memset( dst, 0, sizeof(*dst) );
    
    dst->chargePerUnitScaling.commodityScale = buf_request[5];
    dst->chargePerUnitScaling.priceScale =  buf_request[7];

    AXDRDecodeWord( &buf_request[11], &dst->commodityReference.classId );    
    memcpy( (u8*)&dst->commodityReference.logicalName, &buf_request[15], 6 );
    dst->commodityReference.attributeIndex = buf_request[22];

    pos_in_buf_request = 28;
    for( u8 i = 0; i < lenArray; ++i )
    {
        uint8_t lenIndex = buf_request[pos_in_buf_request++];
        memcpy( dst->chargeTableElement[i].index, &buf_request[pos_in_buf_request], lenIndex );
        pos_in_buf_request += lenIndex + 1;
        
        AXDRDecodeShort( &buf_request[pos_in_buf_request], &dst->chargeTableElement[i].chargePerUnit );
        pos_in_buf_request += eDTL_Long;

        pos_in_buf_request += eDTL_Structure + eDTL_Integer + eDTL_OctetString;
    }    
    
    return Success;
}

uint8_t PaymentChargeClass::SetAttr6( uint8_t* buf_request )
{
    u8 result = SetPaymentChargeUnitCharge( buf_request, &chargeCfg->unitChargePassive );
    if( result == Success )
    {
        /* save in the flash the passive_unit_charge */
        const u8 lenBufUnitCharge = eDTL_Integer + eDTL_Integer + eDTL_LongUnsigned + 6 + eDTL_Integer + MAX_TARIFFS * ( MAX_INDEX_LEN + eDTL_Long );
        BYTE bufUnitCharge[ lenBufUnitCharge ] = {};    
        convertUnitChargeFromStructToMemoryBuffer( bufUnitCharge, &chargeCfg->unitChargePassive );
        FileWrite( ftFile->ftUnitChargePassive, bufUnitCharge );
    }
    
    return result;
}

uint8_t PaymentChargeClass::SetAttr7( uint8_t* buf_request )
{
    /* Check all tags and lengths */
    uint8_t pos_in_buf_request = 0;
    if( buf_request[pos_in_buf_request] != OctetString && buf_request[pos_in_buf_request] != DateTime  )
        return eDAR_TypeUnmatched;
    
    if( buf_request[pos_in_buf_request++] == OctetString )
    {
        if( buf_request[ pos_in_buf_request++ ] != eDTL_DateTime )
            return eDAR_TypeUnmatched;
    }
    
    BYTE bufDateTime[eDTL_DateTime] = {};
    memcpy( bufDateTime, &buf_request[ pos_in_buf_request ], eDTL_DateTime );
    
    if( bufDateTime[0] == 0xff &&
        bufDateTime[1] == 0xff &&
        bufDateTime[2] == 0xff &&
        bufDateTime[3] == 0xff &&
        bufDateTime[4] == 0xff &&
        bufDateTime[5] == 0xff &&
        bufDateTime[6] == 0xff &&
        bufDateTime[7] == 0xff &&
        bufDateTime[8] == 0xff &&
        bufDateTime[9] == 0xff &&
        bufDateTime[10] == 0xff &&
        bufDateTime[11] == 0xff )
    {
        /* Clear previous DateTime */
        memset( &chargeCfg->unitChargeActivationTime, 0, sizeof( chargeCfg->period ) );
        
        /* Save all 0xff */
        memcpy( &chargeCfg->unitChargeActivationTime, bufDateTime, sizeof( bufDateTime ) ); 
        
        return eDAR_Success;
    }
    
    TDateTime tmpStructDateTime;
    memcpy( &tmpStructDateTime, bufDateTime, sizeof( tmpStructDateTime ) );
    
    if( !IsValidWildcard( &tmpStructDateTime ) ||
        (tmpStructDateTime.date.year_hi == 0xff && tmpStructDateTime.date.year_low == 0xff) ||
        tmpStructDateTime.date.month == 0xff ||
        tmpStructDateTime.date.day == 0xff ||
        tmpStructDateTime.time.hour == 0xff ||
        tmpStructDateTime.time.minute == 0xff ||
        tmpStructDateTime.time.second == 0xff ||
        tmpStructDateTime.time.hundredths == 0xff )
        return eDAR_OtherReason;
    
    /* Clear previous DateTime */
    memset( &chargeCfg->unitChargeActivationTime, 0, sizeof( chargeCfg->unitChargeActivationTime ) );
    
    /* Save new DateTime */
    memcpy( &chargeCfg->unitChargeActivationTime, &tmpStructDateTime, sizeof( tmpStructDateTime ) );
  
    DWORD seconds = PackTdateTime( &chargeCfg->unitChargeActivationTime );
    FileWrite( ftFile->ftUnitChargeActivationTime, &seconds );
    
    TDateTime currDateTime;
    GetLocalTime_APDU( &currDateTime );
    if( ClockCompareDateTime( &chargeCfg->unitChargeActivationTime, &currDateTime ) <= 0 )
    {
        ActivatePassiveUnitCharge();
    }
    
    return eDAR_Success;
}

uint8_t PaymentChargeClass::SetAttr8( uint8_t* buf_request )
{
    if( buf_request[0] != DoubleLongUnsigned )
        return eDAR_TypeUnmatched;
    
    u32 tmpPeriod = 0;    
    AXDRDecodeDword( &buf_request[1], &tmpPeriod );
    
    if( tmpPeriod > 60 || tmpPeriod == 0 )                /* 4tobi simetirovati sbor po kajdomu kWh, period doljen biti malenikim - ne boli6e minuti */
        return eDAR_OtherReason;
    
    chargeCfg->period = tmpPeriod;
    
    FileWrite( ftFile->ftPeriod, &chargeCfg->period );
    
    return eDAR_Success;
}

uint8_t PaymentChargeClass::SetAttr9( uint8_t* buf_request )
{
    if( buf_request[0] != BitString )
        return eDAR_TypeUnmatched;
    
    if( buf_request[1] > 8 )
        return eDAR_TypeUnmatched;
    
    chargeCfg->chargeConfiguration = buf_request[2];
    
    return eDAR_Success;
}

uint8_t PaymentChargeClass::SetAttr13( uint8_t* buf_request )
{
    if( buf_request[0] != LongUnsigned )
        return TypeUnmatched;
    
    u16 value;
    AXDRDecodeWord( &buf_request[1], &value );
    
    if( value > 10000 )                 // The range 0x0000 to 0x2710 ( 0 to 10,000 ) represents 0 to 100%.
        return TypeUnmatched;
    
    chargeCfg->proportion = value;
    
    return Success;
}

uint8_t PaymentChargeClass::Set( uint8_t attrID, uint8_t* buf_request )
{
    switch( attrID )
    {
        case PaymentCreditCreditTypeAttr:                       return SetAttr3( buf_request );
        case PaymentCreditPriorityAttr:                         return SetAttr4( buf_request );
        case PaymentChargeUnitChargePassiveAttr:                return SetAttr6( buf_request );
        case PaymentChargeUnitChargeActivationTimeAttr:         return SetAttr7( buf_request );
        case PaymentChargePeriodAttr:                           return SetAttr8( buf_request );
        case PaymentCreditPresetCreditAmountAttr:               return SetAttr9( buf_request );
        case PaymentChargeProportionAttr:                       return SetAttr13( buf_request );
        default:                                                return eDAR_ObjectUndefined;    
    }
}

/*********************************************/
/** GET and SET of PaymentTokenGatewayClass **/
/*********************************************/
bool PaymentTokenGatewayClass::GetAttr2( uint8_t* buf_response, uint16_t& len_response ) const
{
    buf_response[len_response++] = eDT_OctetString;
    
    u8 tokenLen = 0;
    
    if( lastToken.tokenID != 0 )        /* If wasn't received token assume that tokenID == 0 */
    {
        switch (currValues.token[(u8)commonFieldPosInToken::subtype -2])
        {
        case (u8)inTokenSubtype::startPaidToken:        tokenLen = (u8)inTokenLen::startPaid; break;
        case (u8)inTokenSubtype::topUpToken:            tokenLen = (u8)inTokenLen::topUp; break;
        case (u8)inTokenSubtype::stopPaidToken:         tokenLen = (u8)inTokenLen::stopPaid; break;
        case (u8)inTokenSubtype::startNonPaidToken:     tokenLen = (u8)inTokenLen::startNonPaid; break;
        case (u8)inTokenSubtype::stopNonPaidToken:      tokenLen = (u8)inTokenLen::stopNonPaid; break;
        }
    }
    
    buf_response[len_response++] = tokenLen;
    
    memcpy( &buf_response[len_response], currValues.token, tokenLen );
    len_response += tokenLen;
    
    return true;
}

bool PaymentTokenGatewayClass::GetAttr3( uint8_t* buf_response, uint16_t& len_response ) const
{
    buf_response[len_response++] = eDT_DateTime;
    memcpy( &buf_response[len_response], &currValues.tokenTime, eDTL_DateTime );
    len_response += eDTL_DateTime;
    
    return true;
}

bool PaymentTokenGatewayClass::GetAttr4( uint8_t* buf_response, uint16_t& len_response ) const
{
    buf_response[len_response++] = eDT_Array;
    
    if( lastToken.tokenID == 0 )        /* If wasn't received token assume that tokenID == 0 */
    {
        buf_response[len_response++] = 0;
        return true;
    }
    
    buf_response[len_response++] = MAX_LEN_TOKEN_DESCRIPTION_ARRAY;
    
    buf_response[len_response++] = eDT_OctetString;
    buf_response[len_response++] = 1;
    buf_response[len_response++] = (u8)lastToken.subtype;
    
    buf_response[len_response++] = eDT_OctetString;
    buf_response[len_response++] = eDTL_DoubleLongUnsigned;
    AXDREncodeDword( &buf_response[len_response], lastToken.tokenID );
    len_response += eDTL_DoubleLongUnsigned;
    
    buf_response[len_response++] = eDT_OctetString;
    buf_response[len_response++] = LEN_ACTIVE_TRANSACTION_ID;
    memcpy( &buf_response[len_response], &lastToken.activeTransactionID[0], LEN_ACTIVE_TRANSACTION_ID );
    len_response += LEN_ACTIVE_TRANSACTION_ID;
    
    return true;
}

bool PaymentTokenGatewayClass::GetAttr5( uint8_t* buf_response, uint16_t& len_response ) const
{
    buf_response[len_response++] = eDT_Enum;
    buf_response[len_response++] = currValues.tokenDeliveryMethod;
    
    return true;
}

bool PaymentTokenGatewayClass::GetAttr6( uint8_t* buf_response, uint16_t& len_response ) const
{
    buf_response[len_response++] = eDT_Structure;
    buf_response[len_response++] = 2;
    
    buf_response[len_response++] = eDT_Enum;
    buf_response[len_response++] = currValues.tokenStatus.statusCode;
    
    buf_response[len_response++] = eDT_BitString;
    buf_response[len_response++] = 8;
    buf_response[len_response++] = currValues.tokenStatus.dataValue;
    
    return true;
}

bool PaymentTokenGatewayClass::GetAttrTokenID( uint8_t* buf_response, uint16_t& len_response ) const
{
    buf_response[len_response++] = eDT_DoubleLongUnsigned;
    AXDREncodeDword( &buf_response[len_response], lastToken.tokenID );
    len_response += eDTL_DoubleLongUnsigned;
    
    return true;
}

bool PaymentTokenGatewayClass::Get( uint8_t attrID, uint8_t* buf_request, uint8_t* buf_response, uint16_t& len_response )
{
    len_response = 0;
    switch( attrID )
    {
        case PaymentTokenGatewayTokenAttr:                      return GetAttr2( buf_response, len_response );
        case PaymentTokenGatewayTokenTimeAttr:                  return GetAttr3( buf_response, len_response );
        case PaymentTokenGatewayTokenDescriptionAttr:           return GetAttr4( buf_response, len_response );
        case PaymentTokenGatewayTokenDeliveryMethodAttr:        return GetAttr5( buf_response, len_response );
        case PaymentTokenGatewayTokenStatusAttr:                return GetAttr6( buf_response, len_response );
        case PaymentTokenGatewayTokenID:                        return GetAttrTokenID( buf_response, len_response );
        default:                                                return false;
    }
}

/*********************************************/
/**** ACTION of PaymentTokenGatewayClass *****/
/*********************************************/

void PaymentTokenGatewayClass::Action( uint8_t attrID, uint8_t* buf_request, uint8_t* buf_response, uint16_t& len_response )
{
    len_response = 0;
    switch( attrID )
    {
        case PaymentTokenGatewayEnter:
            buf_response[len_response++] = eAR_Success;
            buf_response[len_response++] = 0x01;
            buf_response[len_response++] = 0x00;
            
            buf_response[len_response++] = eDT_Structure;
            buf_response[len_response++] = 2;
            buf_response[len_response++] = eDT_Enum;
            buf_response[len_response++] = Enter( buf_request + 1 );
            buf_response[len_response++] = eDT_BitString;
            buf_response[len_response++] = 8;
            buf_response[len_response++] = 0;            
            break;
        default:
            buf_response[len_response++] = eAR_ObjectUndefined;
            buf_response[len_response++] = 0x00;
    }

    return;
}

/************************************************************************************************/
/************************* Functions of Assist classes *****************************************/
/************************************************************************************************/
void OutTokenClass::UpdateValue( inTokenSubtype inSubtype, u32 tokenID, const BYTE* transactionID, u32 startTime, u8 startTimeStatus )
{
  BYTE outToken[(u8)outTokenLen::max] = {};
  
  outToken[(u8)commonFieldPosOutToken::type] = outTokenType;
  
#warning: "Need to get somewhere tx invocation counter"
  u32 txInvocationCounter = 1;
  AXDREncodeDword(  &outToken[(u8)commonFieldPosOutToken::txInvocCounter], txInvocationCounter );
  
  outToken[(u8)commonFieldPosOutToken::subtype] = (u8)inSubtype;                /* The numeric representation of out token subtype = in token subtype */

  AXDREncodeDword(  &outToken[(u8)commonFieldPosOutToken::tokenID], tokenID );
  
  memcpy( &outToken[(u8)commonFieldPosOutToken::transactionID], transactionID, LEN_ACTIVE_TRANSACTION_ID );
  
  memcpy( &outToken[(u8)commonFieldPosOutToken::startTime], &startTime, sizeof(startTime) );
  outToken[(u8)commonFieldPosOutToken::startTimeStatus] = startTimeStatus;
  
  u32 currDateTimeUTCSec = getCurrentUTCSecondsWithCorrection();
  memcpy( &outToken[(u8)commonFieldPosOutToken::tokenTime], (u8*)&currDateTimeUTCSec, sizeof(currDateTimeUTCSec) );
#warning: "Need to clarify time status"
  outToken[(u8)commonFieldPosOutToken::tokenTimeStatus] = 0xFF;
  
  /* Current Asum RG value */
  u64 currValueAsumRg = GetValueFromRegister( &RegisterAsumLN );
  memcpy( &outToken[(u8)commonFieldPosOutToken::activeEnergy], (u8*)&currValueAsumRg, sizeof(currValueAsumRg) );
  
  u32 consumedKWh = ConsumedKWhFromStartObject.GetConsumedKWh();
  memcpy( &outToken[(u8)commonFieldPosOutToken::usedEnergy], (u8*)&consumedKWh, sizeof(consumedKWh) );
  
  BYTE bufStates[6] = {};
  InternalGetRequest( cicDataClassID, &cicDataState1BaseLN, 2, NULL, bufStates );
  if( bufStates[0] == eDAR_Success && bufStates[1] == eDT_DoubleLongUnsigned )
  {
      memcpy( &outToken[(u8)commonFieldPosOutToken::states], &bufStates[2], eDTL_DoubleLongUnsigned );
  }
  else
  {
      memset( &outToken[(u8)commonFieldPosOutToken::states], 0x00, eDTL_DoubleLongUnsigned );
  }

  BYTE bufAlarms[6] = {};
  InternalGetRequest( cicDataClassID, &cicDataAlarmBaseLN, 2, NULL, bufAlarms );
  if( bufAlarms[0] == eDAR_Success && bufAlarms[1] == eDT_DoubleLongUnsigned )
  {
      memcpy( &outToken[(u8)commonFieldPosOutToken::alarms], &bufAlarms[2], eDTL_DoubleLongUnsigned );
  }
  else
  {
      memset( &outToken[(u8)commonFieldPosOutToken::states], 0x00, eDTL_DoubleLongUnsigned );
  }
  
#warning: "Need to get somewhere AES_GSM"
  BYTE aes_gsm_buf[AES_GSM_TAG_LEN] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
  
  if( inSubtype == inTokenSubtype::startPaidToken ||
      inSubtype == inTokenSubtype::topUpToken ||
      inSubtype == inTokenSubtype::stopPaidToken )
  {
      s32 availableCredit = PaymentImportAccount.GetSumOfAllCurrentCreditAmount();   /* Current acount credit + sum to Top Up this credit */
      memcpy( &outToken[(u8)specificPaidFieldPosOutToken::availableCredit], (u8*)&availableCredit, sizeof(availableCredit) );
      
      s32 totalAmountPaid = PaymentImportAccount.GetSumOfAllChargeTotalAmountPaid();
      memcpy( &outToken[(u8)specificPaidFieldPosOutToken::usedCredit], (u8*)&totalAmountPaid, sizeof(totalAmountPaid) );
      
      memcpy( &outToken[(u8)specificPaidFieldPosOutToken::usedCredit + 4], aes_gsm_buf, sizeof(aes_gsm_buf) );
  }
  else
  {
      memcpy( &outToken[(u8)commonFieldPosOutToken::alarms + 4], aes_gsm_buf, sizeof(aes_gsm_buf) );
  }

  FileWrite( ftOutToken_Token, outToken );
}

OutTokenClass::OutTokenClass( const LOGICAL_NAME* const _ln ) : ln( _ln )
{
#ifndef NEW_CONST_CLASS_MAP                                                                           
    DataObjectsMap[(LOGICAL_NAME*)_ln] = this; 
#endif // NEW_CONST_CLASS_MAP
}

#pragma optimize = none         /* DON'T COMMENT, because with optimization len_response calc wrong */
bool OutTokenClass::GetAttr2( uint8_t* buf_response, uint16_t& len_response ) const
{
    buf_response[len_response++] = eDT_OctetString;
  
    BYTE outToken[(u8)outTokenLen::max] = {};
  
    if( FileRead( ftOutToken_Token, outToken ) == 0 )
    {
        buf_response[len_response++] = 0;
        return true;
    }
    
    u8 outTokenLen = 0;
    switch (outToken[(u8)commonFieldPosOutToken::subtype])
    {
    case (u8)outTokenSubtype::startPaidResponse:        outTokenLen = (u8)outTokenLen::startPaidResponse; break;
    case (u8)outTokenSubtype::topUpResponse:            outTokenLen = (u8)outTokenLen::topUpResponse; break;
    case (u8)outTokenSubtype::stopPaidResponse:         outTokenLen = (u8)outTokenLen::stopPaidResponse; break;
    case (u8)outTokenSubtype::startNonPaidResponse:     outTokenLen = (u8)outTokenLen::startNonPaidResponse; break;
    case (u8)outTokenSubtype::stopNonPaidResponse:      outTokenLen = (u8)outTokenLen::stopNonPaidResponse; break;
    }
    
    buf_response[len_response++] = outTokenLen;
    
    memcpy( &buf_response[len_response], outToken, outTokenLen );
    
    len_response += outTokenLen;
    
    return true;
}

bool OutTokenClass::Get( uint8_t attrID, uint8_t* buf_request, uint8_t* buf_response, uint16_t& len_response )
{
    len_response = 0;
    switch( attrID )
    {
        case ValueAttr: return GetAttr2( buf_response, len_response );
        default:        return false;
    }
}

ActiveTransactionIDClass::ActiveTransactionIDClass( const LOGICAL_NAME* const _ln ) : ln( _ln )
{
#ifndef NEW_CONST_CLASS_MAP                                                                           
    DataObjectsMap[(LOGICAL_NAME*)_ln] = this; 
#endif // NEW_CONST_CLASS_MAP
}

bool ActiveTransactionIDClass::GetAttr2( uint8_t* buf_response, uint16_t& len_response ) const
{
    buf_response[len_response++] = eDT_OctetString;
    buf_response[len_response++] = LEN_ACTIVE_TRANSACTION_ID;
    memcpy( &buf_response[len_response], TokenGatewayForImportAccount.GetActiveTransactionID(), LEN_ACTIVE_TRANSACTION_ID );
    len_response += LEN_ACTIVE_TRANSACTION_ID;
    
    return true;
}

bool ActiveTransactionIDClass::Get( uint8_t attrID, uint8_t* buf_request, uint8_t* buf_response, uint16_t& len_response )
{
    len_response = 0;
    switch( attrID )
    {
        case ValueAttr: return GetAttr2( buf_response, len_response );
        default:        return false;
    }
}


TopUpsSumClass::TopUpsSumClass( const LOGICAL_NAME* const _ln ) : ln( _ln )
{
#ifndef NEW_CONST_CLASS_MAP                                                                           
    RegisterObjectsMap[(LOGICAL_NAME*)_ln] = this; 
#endif // NEW_CONST_CLASS_MAP
}

bool TopUpsSumClass::GetAttr2( uint8_t* buf_response, uint16_t& len_response ) const
{
    buf_response[len_response++] = eDT_DoubleLong;

    /* sei4as na s4etu + potra4eno */
    s32 startTopUpSum = PaymentImportAccount.GetSumOfAllCurrentCreditAmount() + PaymentImportAccount.GetSumOfAllChargeTotalAmountPaid();
    AXDREncodeDoubleLong( &buf_response[len_response], startTopUpSum );
    len_response += eDTL_DoubleLong;
    
    return true;
}

bool TopUpsSumClass::GetAttr3( uint8_t* buf_response, uint16_t& len_response ) const
{
    buf_response[len_response++] = eDT_Structure;
    buf_response[len_response++] = 2;
    
    buf_response[len_response++] = eDT_Integer;
    buf_response[len_response++] = PaymentImportAccount.GetCurrencyScale();
    
    buf_response[len_response++] = eDT_Enum;
    buf_response[len_response++] = UnitCurrency;
    
    return true;
}

bool TopUpsSumClass::Get( uint8_t attrID, uint8_t* buf_request, uint8_t* buf_response, uint16_t& len_response )
{
    len_response = 0;
    switch( attrID )
    {
        case ValueAttr:         return GetAttr2( buf_response, len_response );
        case ScalerUnitAttr:    return GetAttr3( buf_response, len_response );
        default:                return false;
    }
}

TotalAmountPaidClass::TotalAmountPaidClass( const LOGICAL_NAME* const _ln ) : ln( _ln )
{
#ifndef NEW_CONST_CLASS_MAP                                                                           
    RegisterObjectsMap[(LOGICAL_NAME*)_ln] = this; 
#endif // NEW_CONST_CLASS_MAP
}

bool TotalAmountPaidClass::GetAttr2( uint8_t* buf_response, uint16_t& len_response ) const
{
    buf_response[len_response++] = eDT_DoubleLong;

    /* potra4eno */
    s32 totalAmountPaid = PaymentImportAccount.GetSumOfAllChargeTotalAmountPaid();
    AXDREncodeDoubleLong( &buf_response[len_response], totalAmountPaid );
    len_response += eDTL_DoubleLong;
    
    return true;
}

bool TotalAmountPaidClass::GetAttr3( uint8_t* buf_response, uint16_t& len_response ) const
{
    buf_response[len_response++] = eDT_Structure;
    buf_response[len_response++] = 2;
    
    buf_response[len_response++] = eDT_Integer;
    buf_response[len_response++] = PaymentImportAccount.GetCurrencyScale();
    
    buf_response[len_response++] = eDT_Enum;
    buf_response[len_response++] = UnitCurrency;
    
    return true;
}

bool TotalAmountPaidClass::Get( uint8_t attrID, uint8_t* buf_request, uint8_t* buf_response, uint16_t& len_response )
{
    len_response = 0;
    switch( attrID )
    {
        case ValueAttr:         return GetAttr2( buf_response, len_response );
        case ScalerUnitAttr:    return GetAttr3( buf_response, len_response );
        default:                return false;
    }
}

ConsumedKWhFromStartClass::ConsumedKWhFromStartClass( const LOGICAL_NAME* const _ln ) : ln( _ln ), kWhWhenStart(0)
{
#ifndef NEW_CONST_CLASS_MAP                                                                           
    RegisterObjectsMap[(LOGICAL_NAME*)_ln] = this; 
#endif // NEW_CONST_CLASS_MAP
}

void ConsumedKWhFromStartClass::Init()
{
    u64 tmpkWhWhenStart = 0;
    if( FileRead( ftConsumedKWhFromStart_KWhWhenStart, &tmpkWhWhenStart ) != 0 )
    {
        kWhWhenStart = tmpkWhWhenStart;
    }
}

void ConsumedKWhFromStartClass::UpdateKWhWhenStart()
{
    kWhWhenStart = GetValueFromRegister( &RegisterAsumLN );
    /* Save new value in ft */
    FileWrite( ftConsumedKWhFromStart_KWhWhenStart, &kWhWhenStart );
}

u32 ConsumedKWhFromStartClass::GetConsumedKWh() const
{
    u64 currKWhValue = GetValueFromRegister( &RegisterAsumLN );
    return (currKWhValue - kWhWhenStart);
}

bool ConsumedKWhFromStartClass::GetAttr2( uint8_t* buf_response, uint16_t& len_response ) const
{
    buf_response[len_response++] = eDT_DoubleLongUnsigned;
    
    u32 consumedKWh = GetConsumedKWh();

    AXDREncodeDword( &buf_response[len_response], consumedKWh );
    len_response += eDTL_DoubleLongUnsigned;
    
    return true;
}

bool ConsumedKWhFromStartClass::GetAttr3( uint8_t* buf_response, uint16_t& len_response ) const
{
    buf_response[len_response++] = eDT_Structure;
    buf_response[len_response++] = 2;
    
    buf_response[len_response++] = eDT_Integer;
    buf_response[len_response++] = GetScalerOfValueFromRegister( &RegisterAsumLN );
    
    buf_response[len_response++] = eDT_Enum;
    buf_response[len_response++] = UnitWh;
    
    return true;
}

bool ConsumedKWhFromStartClass::Get( uint8_t attrID, uint8_t* buf_request, uint8_t* buf_response, uint16_t& len_response )
{
    len_response = 0;
    switch( attrID )
    {
        case ValueAttr:         return GetAttr2( buf_response, len_response );
        case ScalerUnitAttr:    return GetAttr3( buf_response, len_response );
        default:                return false;
    }
}

TokenIDClass::TokenIDClass( const LOGICAL_NAME* const _ln ) : ln( _ln )
{
#ifndef NEW_CONST_CLASS_MAP                                                                           
    DataObjectsMap[(LOGICAL_NAME*)_ln] = this; 
#endif // NEW_CONST_CLASS_MAP
}

bool TokenIDClass::GetAttr2( uint8_t* buf_response, uint16_t& len_response ) const
{
    buf_response[len_response++] = eDT_DoubleLongUnsigned;

    u32 TID = TokenGatewayForImportAccount.GetTokenID();
    AXDREncodeDword( &buf_response[len_response], TID );
    len_response += eDTL_DoubleLongUnsigned;
    
    return true;
}

bool TokenIDClass::Get( uint8_t attrID, uint8_t* buf_request, uint8_t* buf_response, uint16_t& len_response )
{
    len_response = 0;
    switch( attrID )
    {
        case ValueAttr:         return GetAttr2( buf_response, len_response );
        default:                return false;
    }
}


ExpiresTimeClass::ExpiresTimeClass( const LOGICAL_NAME* const _ln ) : ln( _ln )
{
#ifndef NEW_CONST_CLASS_MAP                                                                           
    DataObjectsMap[(LOGICAL_NAME*)_ln] = this; 
#endif // NEW_CONST_CLASS_MAP
}

bool ExpiresTimeClass::GetAttr2( uint8_t* buf_response, uint16_t& len_response ) const
{
    buf_response[len_response++] = eDT_OctetString;
    buf_response[len_response++] = eDTL_DateTime;

    u32 expiresTimeSec = TokenGatewayForImportAccount.GetExpiresTimeSec();
    u8 expiresTimeStatus = TokenGatewayForImportAccount.GetExpiresTimeStatus();
    
    TDateTime expiresTime;
    SecondsUTC_To_Local_DateTime_WithCorrection( expiresTimeSec, &expiresTime );    
    memcpy( &buf_response[len_response], &expiresTime, eDTL_DateTime );
    len_response += eDTL_DateTime - 1;
    
    buf_response[len_response++] = expiresTimeStatus;
    
    return true;
}

bool ExpiresTimeClass::Get( uint8_t attrID, uint8_t* buf_request, uint8_t* buf_response, uint16_t& len_response )
{
    len_response = 0;
    switch( attrID )
    {
        case ValueAttr:         return GetAttr2( buf_response, len_response );
        default:                return false;
    }
}

#endif // _PAYMENT_