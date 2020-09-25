/*
    \file Payment.h

    \author Mihailovskii G.
    
    \date 2020
*/

#if !defined _PAYMENT_
#define _PAYMENT_

#include "config.h"
#include "CommonTypes.h"
#include "core.h"
#include "cicData.h"

static const uint8_t MAX_OBJECTS_IN_CREDIT_REF_LIST     = 1;
static const uint8_t MAX_OBJECTS_IN_CHARGE_REF_LIST     = 1;
static const uint8_t MAX_OBJECTS_IN_CREDIT_CHARGE_CFG   = 1;
static const uint8_t MAX_OBJECTS_IN_TOKEN_GATEWAY_CFG   = 1;

static const uint8_t MAX_TARIFFS                        = 6;
static const uint8_t MAX_INDEX_LEN                      = 1;     // max len of index in charge_table_element

static const uint8_t MAX_LEN_CURRENCY_NAME              = 3;       

static const uint8_t MAX_LEN_RECEIVED_TOKEN             = 106;  // startPaidToken
static const uint8_t MAX_LEN_TOKEN_DESCRIPTION_ELEMENT  = 1;
static const uint8_t MAX_LEN_TOKEN_DESCRIPTION_ARRAY    = 3;
static const uint8_t NUM_OF_STORED_TOKENS_ID            = 200;   // kol-vo sohraneaemih TID
static const uint8_t AES_GSM_TAG_LEN                    = 12;

static const uint8_t LEN_ACTIVE_TRANSACTION_ID          = 16;
static const uint8_t LEN_KEY_EK                         = 24;
static const uint8_t LEN_KEY_AK                         = 24;

const u16 cicPaymentAccountClassID		        = 111;
const u16 cicPaymentCreditClassID		        = 112;
const u16 cicPaymentChargeClassID		        = 113;
const u16 cicPaymentTokenGatewayClassID                 = 115;

//const LOGICAL_NAME PaymentImportAccountLn               = {0, 0, 19, 0, 0, 255};
//const LOGICAL_NAME PaymentImportCreditLn                = {0, 0, 19, 10, 0, 255};
//const LOGICAL_NAME PaymentActiveImportChargeLn          = {0, 0, 19, 20, 0, 255};
//const LOGICAL_NAME PaymentImportTokenGatewayLn          = {0, 0, 19, 40, 0, 255};

//const LOGICAL_NAME ActiveTransactionIDLn                = {0, 0, 96, 60, 31, 255};
//const LOGICAL_NAME TopUpsSumLn                          = {0, 0, 96, 60, 32, 255};
//const LOGICAL_NAME TotalAmountPaidLn                    = {0, 0, 96, 60, 33, 255};
//const LOGICAL_NAME ExpiresTimeLn                        = {0, 0, 96, 60, 34, 255};
//const LOGICAL_NAME OutTokenLn                           = {0, 0, 96, 60, 35, 255};
//const LOGICAL_NAME ConsumedKWhFromStartLn               = {1, 0, 15, 9, 0, 255};
//const LOGICAL_NAME TokenIDLn                            = {0, 0, 128, 8, 0, 255};

/*********************************************/
/************ Account's attributes ***********/
/*********************************************/

enum PaymentAccountAttributes{
    PaymentAccountModeAndStatusAttr                     = 2,
    PaymentAccountCurrentCreditInUseAttr                = 3,
    PaymentAccountCurrentCreditStatusAttr               = 4,
    PaymentAccountAvailableCreditAttr                   = 5,
    PaymentAccountAmountToClearAttr                     = 6,
    PaymentAccountClearanceThresholdAttr                = 7,
    PaymentAccountAggregatedDebtAttr                    = 8,
    PaymentAccountCreditReferenceListAttr               = 9,
    PaymentAccountChargeReferenceListAttr               = 10,
    PaymentAccountCreditChargeConfigurationAttr         = 11,
    PaymentAccountTokenGatewayConfigurationAttr         = 12,
    PaymentAccountAccountActivationTimeAttr             = 13,
    PaymentAccountAccountClosureTimeAttr                = 14,
    PaymentAccountCurrencyAttr                          = 15,
    PaymentAccountLowCreditThresholdAttr                = 16,
    PaymentAccountNextCreditAvailableThresholdAttr      = 17,
    PaymentAccountMaxProvisionAttr                      = 18,
    PaymentAccountMaxProvisionPeriodAttr                = 19,  
};
enum PaymentAccountMethods{
    PaymentAccountActivateAccount                       = 1,
    PaymentAccountCloseAccount                          = 2,
    PaymentAccountResetAccount                          = 3,
};

/* account_mode_and_status */
enum eT_payment_mode{
    creditMode          = 1,
    prepaymentMode      = 2
};
enum eT_account_status{
    newAccount          = 1,
    activeAccount       = 2,
    closedAccount       = 3
};

typedef struct{
    eT_payment_mode     paymentMode;
    eT_account_status   accountStatus;
} PaymentAccountModeAndStatus;

/* current_credit_status */
static const uint8_t creditStatusInCredit               = 0x01;
static const uint8_t creditStatusLowCredit              = 0x02;
static const uint8_t creditStatusNextCreditEnabled      = 0x04;
static const uint8_t creditStatusNextCreditSelectable   = 0x08;
static const uint8_t creditStatusNextCreditSelected     = 0x10;
static const uint8_t creditStatusSelectableCreditInUse  = 0x20;
static const uint8_t creditStatusOutOfCredit            = 0x40;

/* credit_charge_configuration */
typedef struct{
    const LOGICAL_NAME  *creditRef;
    const LOGICAL_NAME  *chargeRef;
    u8                  collectionCfg;
} PaymentAccountCreditChargeCfgElement;

/* collection_configuration */
static const uint8_t collectWhenSupplyDisconected       = 0x01;
static const uint8_t collectInLoadLimitingPeriods       = 0x02;
static const uint8_t collectInFriendlyCreditPeriods     = 0x04;

/* token_gateway_configuration */
typedef struct{
    LOGICAL_NAME        creditRef;
    u8                  tokenProportion;        // in percentage
} PaymentAccountTokenGatewayCfgElement;

/* currency */
enum eT_currency_unit{
    currencyUnitTime,
    currencyUnitConsuption,
    currencyUnitMonetary
};

typedef struct{
    u8                  name[MAX_LEN_CURRENCY_NAME];        // utf8-string
    s8                  scale;                              // integer
    eT_currency_unit    unit;
}PaymentAccountCurency;

/* Account's Configuration */
typedef struct{
    PaymentAccountModeAndStatus                 modeAndStatus;                  // 2
    const LOGICAL_NAME*                         creditRefList[MAX_OBJECTS_IN_CREDIT_REF_LIST];          // 9
    const LOGICAL_NAME*                         chargeRefList[MAX_OBJECTS_IN_CHARGE_REF_LIST];          // 10
    PaymentAccountCreditChargeCfgElement        creditChargeCfg[MAX_OBJECTS_IN_CREDIT_CHARGE_CFG];      // 11
    PaymentAccountTokenGatewayCfgElement        tokenGatewayCfg[MAX_OBJECTS_IN_TOKEN_GATEWAY_CFG];      // 12
    TDateTime                                   accountActivationTime;          // 13
    TDateTime                                   accountClosureTime;             // 14
    PaymentAccountCurency                       currency;                       // 15
    s32                                         maxProvisionPeriod;             // 19
	s32                                         clearanceThreshold;             // 7
	u16                                         maxProvision;                   // 18
} PaymentAccountCfg;

/* Account's dynamic values */
typedef struct{
    s32         availableCredit;                // 5. sum of available posetive Credits
    s32         amountToClear;                  // 6. sum of available negative Credits
    s32         aggregatedDebt;                 // 8. read from Charges
    s32         lowCreditThreshold;             // 16. read from active Credit
    s32         nextCreditAvailableThreshold;   // 17. read from next Credit
	u8          currCreditInUse;                // 3. read from credit_reference_list
    u8          currCreditStatus;               // 4. read from Credit and next Credit
} PaymentAccountDynamicValues;

typedef __packed struct {
    const uint16_t    ftAccountStatus;
    const uint16_t    ftAccountActivationTime;
    const uint16_t    ftAccountClosureTime;
    const uint16_t    ftMaxProvision;
    const uint16_t    ftMaxProvisionPeriod;
    const uint16_t    ftCurrency;
} ftPaymentAccount;

/*********************************************/
/************ Credit's attributes ************/
/*********************************************/

enum PaymentCreditAttributes{
    PaymentCreditCurrentCreditAmountAttr                = 2,
    PaymentCreditCreditTypeAttr                         = 3,
    PaymentCreditPriorityAttr                           = 4,
    PaymentCreditWarningThresholdAttr                   = 5,
    PaymentCreditLimitAttr                              = 6,
    PaymentCreditCreditConfigurationAttr                = 7,
    PaymentCreditCreditStatusAttr                       = 8,
    PaymentCreditPresetCreditAmountAttr                 = 9,
    PaymentCreditCreditAvailableThresholdAttr           = 10,
    PaymentCreditPeriodAttr                             = 11,
};
enum PaymentCreditMethods{
    PaymentCreditUpdateAmount                           = 1,
    PaymentCreditSetAmountToValue                       = 2,
    PaymentCreditInvokeCredit                           = 3,
};

/* credit_type */
enum eT_creditType{
    tokenCredit,
    reservedCredit,
    emergencyCredit,
    timeBasedCredit,
    consumptionBasedCredit
};

/* credit_configuration */
static const uint8_t creditCfgVisualInd                 = 0x01;         // requires visual indication
static const uint8_t creditCfgConfirmation              = 0x02;         // requires confirmation before it can be selected/invoked
static const uint8_t creditCfgRepayment                 = 0x04;         // requires the credit amount to be paid back
static const uint8_t creditCfgResettable                = 0x08;         // resettable
static const uint8_t creditCfgReceiveCreditToken        = 0x10;         // able to receive credit amounts from tokens

/* credit_status */
enum eT_creditStatus{
    ENABLED,
    SELECTABLE,
    SELECTED,
    IN_USE,
    EXHAUSTED
};

/* Credit's Configuration */
typedef struct{
	TDateTime           period;                         // 11
    s32                 warningThreshold;               // 5
    s32                 limit;                          // 6
    s32                 presetCreditAmount;             // 9
    s32                 creditAvailableThreshold;       // 10
	eT_creditType       creditType;                     // 3
    u8                  priority;                       // 4
	u8                  creditConfiguration;            // 7
} PaymentCreditCfg;

typedef struct{
    s32                 currentCreditAmount;            // 2    
    eT_creditStatus     creditStatus;                   // 8
} PaymentCreditDynamicValues;

typedef __packed struct {
    const uint16_t    ftCurrentCreditAmount;
    const uint16_t    ftWarningThreshold;
    const uint16_t    ftLimit;
    const uint16_t    ftCreditStatus;
} ftPaymentCredit;

/*********************************************/
/************ Charge's attributes ************/
/*********************************************/

enum PaymentChargeAttributes{
    PaymentChargeTotalAmountPaidAttr                    = 2,
    PaymentChargeChargeTypeAttr                         = 3,
    PaymentChargePriorityAttr                           = 4,
    PaymentChargeUnitChargeActiveAttr                   = 5,
    PaymentChargeUnitChargePassiveAttr                  = 6,
    PaymentChargeUnitChargeActivationTimeAttr           = 7,
    PaymentChargePeriodAttr                             = 8,
    PaymentChargeChargeConfigurationAttr                = 9,
    PaymentChargeLastCollectionTimeAttr                 = 10,
    PaymentChargeLastCollectionAmountAttr               = 11,
    PaymentChargeTotalAmountRemainingAttr               = 12,
    PaymentChargeProportionAttr                         = 13
};
enum PaymentChargeMethods{
    PaymentChargeUpdateUnitCharge                       = 1,
    PaymentChargeActivatePassiveUnitCharge              = 2,
    PaymentChargeCollect                                = 3,
    PaymentChargeUpdateTotalAmountRemaining             = 4,
    PaymentChargeSetTotalAmountRemaining                = 5,
};

/* charge_type */
enum eT_PaymentChargeType{
    PaymentChargeConsumptionBased                       = 0,
    PaymentChargeTimeBased                              = 1,
    PaymentChargePaymentEventBased                      = 2,
};      

/* unit_charge_active and  unit_charge_passive */
typedef struct{
    s8                  commodityScale;
    s8                  priceScale;
} chargePerUnitScalingType;

typedef struct{
    LOGICAL_NAME        logicalName;
	u16                 classId;
    s8                  attributeIndex;
} commodityReferenceType;

typedef struct{
    BYTE                 index[MAX_INDEX_LEN];                                  // имя тарифа (week_id+day_id)
    s16                  chargePerUnit;
 } chargeTableElementType;

typedef struct{
    chargePerUnitScalingType    chargePerUnitScaling;
    commodityReferenceType      commodityReference;
    chargeTableElementType      chargeTableElement[MAX_TARIFFS];                // максимум 10 тарифов
} PaymentChargeUnitCharge;

/* charge_configuration */
const uint8_t chargePercentageBaseCollection            = 0x01;
const uint8_t chargeContinuousCollection                = 0x02;

/* Charge's Configuration */
typedef struct{
    PaymentChargeUnitCharge     unitChargeActive;               // 5
    PaymentChargeUnitCharge     unitChargePassive;              // 6
    TDateTime                   unitChargeActivationTime;       // 7
	u16                         proportion;                     // 13
    u32                         period;                         // 8
	eT_PaymentChargeType        chargeType;                     // 3
    u8                          priority;                       // 4
    u8                          chargeConfiguration;            // 9
} PaymentChargeCfg;

typedef struct{
	TDateTime                   lastCollectionTime;             // 10
    s32                         totalAmountPaid;	        // 2
    s32                         lastCollectionAmount; 	        // 11
    s32                         totalAmountRemaining;           // 12
} PaymentChargeDynamicValues;

typedef __packed struct {
    const uint16_t      ftTotalAmountPaid;
    const uint16_t      ftUnitChargeActive;
    const uint16_t      ftUnitChargePassive;
    const uint16_t      ftUnitChargeActivationTime;
    const uint16_t      ftPeriod;
    const uint16_t      ftLastCollectionTime;
    const uint16_t      ftLastCollectionAmount;
    const uint16_t      ftTotalAmountRemaining;
    const uint16_t      ftLastMeasurementValue;
    const uint16_t      ftSumToCollect;
} ftPaymentCharge;

/*********************************************/
/********* Token gateway's attributes ********/
/*********************************************/

enum PaymentTokenGatewayAttributes{
    PaymentTokenGatewayTokenAttr                        = 2,
    PaymentTokenGatewayTokenTimeAttr                    = 3,
    PaymentTokenGatewayTokenDescriptionAttr             = 4,
    PaymentTokenGatewayTokenDeliveryMethodAttr          = 5,
    PaymentTokenGatewayTokenStatusAttr                  = 6,
    PaymentTokenGatewayTokenID                          = 0xff
};
enum PaymentTokenGatewayMethods{
    PaymentTokenGatewayEnter                            = 1,
};

/* token_description */
typedef struct{
    BYTE        tokenDescriptionElement[MAX_LEN_TOKEN_DESCRIPTION_ELEMENT];
} tokenDescriptionStruct;

/* token_delivery_method */
enum eT_PaymentTokenDeliveryMethod{
    remoteTokenDelivery         = 0,            // via remote communications
    localTokenDelivery          = 1,            // via local communications
    manualTokenDelivery         = 2             // via manual entry
};

/* token_status */
enum eT_tokenStatusCode{
    formatOK                    = 0,
    authenticationOK            = 1,
    validationOK                = 2,
    executionOK                 = 3,
    formatFAIL                  = 4,
    authenticationFAIL          = 5,
    validationFAIL              = 6,
    executionFAIL               = 7,
    receivedAndNotYetProcessed  = 8,
};

typedef struct{
    eT_tokenStatusCode  statusCode;
    u8                  dataValue;              // !!! format zavisit ot realizacii
} PaymentTokenStatus;

/* Token Gateway's Configuration */
typedef struct{
    BYTE                                token[MAX_LEN_RECEIVED_TOKEN];                          // dynamic
    TDateTime                           tokenTime;                                              // dynamic
    tokenDescriptionStruct              tokenDescription[MAX_LEN_TOKEN_DESCRIPTION_ARRAY];      // dynamic
    eT_PaymentTokenDeliveryMethod       tokenDeliveryMethod;                                    // dynamic
    PaymentTokenStatus                  tokenStatus;                                            // dynamic
}PaymentTokenGatewayDynamicValues;

enum class inTokenSubtype{
    startPaidToken    = 1,
    topUpToken        = 2,
    stopPaidToken     = 3,
    startNonPaidToken = 4,
    stopNonPaidToken  = 5,
    none
};

/* Token format */
typedef struct{
    BYTE                                activeTransactionID[LEN_ACTIVE_TRANSACTION_ID]; // ADDERA order ID
    u32                                 expiresTimeSecReceivedWithStart;
    u32                                 timeOfStartSec;
    u32                                 tokenID;                                        // id of last received token
    inTokenSubtype                      subtype;
    u8                                  nextReceivedTokenIndex;                         // index in the array for the next received token
    u8                                  expiresTimeStatusReceivedWithStart;
    u8                                  timeOfStartStatus;
}PaymentTokenFormat;  

typedef __packed struct {
    const uint16_t      ftToken;
    const uint16_t      ftTokenTime;
    const uint16_t      ftTokenDeliveryMethod;
    const uint16_t      ftTokenStatusCode;
    const uint16_t      ftTokenID;
    const uint16_t      ftNextReceivedTokenIndex;
    const uint16_t      ftActiveTransactionID;
    const uint16_t      ftLastTokenSubtype;
    const uint16_t      ftExpiresTimeSecReceivedWithStart;
    const uint16_t      ftExpiresTimeStatusReceivedWithStart;
    const uint16_t      ftTimeOfStartSec;
    const uint16_t      ftTimeOfStartStatus;
} ftPaymentTokenGateway;

/***********************************************************************************************************/
/****************************************** Interfaces of Classes ******************************************/
/***********************************************************************************************************/

/*********************************************/
/************* Credit's Interface ************/
/*********************************************/
class PaymentCreditClass : public COSEMInterfaceClassAbstract{
public:
  PaymentCreditClass( const LOGICAL_NAME* const _ln, PaymentCreditCfg* const cfg, const ftPaymentCredit* const _ftFile );
  
  const LOGICAL_NAME* const      ln;
  
  bool Get( uint8_t attrID, uint8_t* buf_request, uint8_t* buf_response, uint16_t& len_response );
  uint8_t Set( uint8_t attrID, uint8_t* buf_request );
  //      void Action( uint8_t attrID, uint8_t* buf_request, uint8_t* buf_response, uint16_t& len_response );
  
  void Init();
  void IdleSecond();
  void IdleMinute();
  
  /* Functions for communication between Payment classes */
  void UpdateAmount( s32 value );               // Blue Book
  void InvokeCredit( s32 data = 0);             // Blue Book
  const LOGICAL_NAME* GetLn() const;
  s32 GetCurrentCreditAmount() const;
  eT_creditType GetCreditType() const;
  u8 GetPriority() const;
  s32 GetWarningThreshold() const;
  eT_creditStatus GetCreditStatus() const;
  s32 GetPresetCreditAmount() const;
  s32 GetCreditAvailableThreshold() const;
  bool GetRequiresCreditAmountToBePaidBack() const;     // credit_configuration bit 2
  bool InvokeCreditStatusToInUse();
  bool InvokeCreditStatusToEnable();
  void ResetCredit();
  
private:
  bool GetAttr2( uint8_t* buf_response, uint16_t& len_response ) const;
  bool GetAttr3( uint8_t* buf_response, uint16_t& len_response ) const;
  bool GetAttr4( uint8_t* buf_response, uint16_t& len_response ) const;
  bool GetAttr5( uint8_t* buf_response, uint16_t& len_response ) const;
  bool GetAttr6( uint8_t* buf_response, uint16_t& len_response ) const;
  bool GetAttr7( uint8_t* buf_response, uint16_t& len_response ) const;
  bool GetAttr8( uint8_t* buf_response, uint16_t& len_response ) const;
  bool GetAttr9( uint8_t* buf_response, uint16_t& len_response ) const;
  bool GetAttr10( uint8_t* buf_response, uint16_t& len_response ) const;      
  bool GetAttr11( uint8_t* buf_response, uint16_t& len_response ) const;
  
  uint8_t SetAttr3( uint8_t* buf_request );
  uint8_t SetAttr4( uint8_t* buf_request );
  uint8_t SetAttr5( uint8_t* buf_request );
  uint8_t SetAttr6( uint8_t* buf_request );
  uint8_t SetAttr7( uint8_t* buf_request );
  uint8_t SetAttr9( uint8_t* buf_request );
  uint8_t SetAttr10( uint8_t* buf_request );
  uint8_t SetAttr11( uint8_t* buf_request );
  /*
  void ActMeth1();
  void ActMeth2();
  void ActMeth3();
  */
  
  s32 SetAmountToValue( s32 newValue );
  
  /* Functions for internal work */
  bool CheckPeriod();
  void ControlCreditStatus();
  
  PaymentCreditCfg* const       creditCfg;
  const ftPaymentCredit* const  ftFile;
  
  PaymentCreditDynamicValues    currValues;
};

/*********************************************/
/************* Charge's Interface ************/
/*********************************************/
class PaymentChargeClass : public COSEMInterfaceClassAbstract{
public:
  PaymentChargeClass( const LOGICAL_NAME* const _ln , PaymentChargeCfg* const cfg, const ftPaymentCharge* const _ftFile );
  
  const LOGICAL_NAME* const     ln;
  
  bool Get( uint8_t attrID, uint8_t* buf_request, uint8_t* buf_response, uint16_t& len_response );
  uint8_t Set( uint8_t attrID, uint8_t* buf_request );
  //      void Action( uint8_t attrID, uint8_t* buf_request, uint8_t* buf_response, uint16_t& len_response );
  
  void Init();
  void IdleSecond();
  void IdleMinute();
  
  /* Functions for communication between Payment classes */
  const LOGICAL_NAME* GetLn() const;
  s32 GetTotalAmountPaid() const;
  u8 GetPriority() const;
  s8 GetPriceScale() const;
  bool GetContinuousCollection() const;
  s32 GetTotalAmountRemaining() const;
  bool GetNewCollection() const;
  s32 GetSumToCollect() const;
  void ConfirmCollection();
  void RefuseCollection();
  void ActivateCharge();
  void CloseCharge();
  void ResetCharge();
  void SetIsLinkedAccountActive( bool newIsLinkedAccountActive );
  void ExecutePaymentEventBasedCollection( s32 topUpSum );
  
private:  
  bool GetAttr2( uint8_t* buf_response, uint16_t& len_response ) const;
  bool GetAttr3( uint8_t* buf_response, uint16_t& len_response ) const;
  bool GetAttr4( uint8_t* buf_response, uint16_t& len_response ) const;
  bool GetAttr5( uint8_t* buf_response, uint16_t& len_response ) const;
  bool GetAttr6( uint8_t* buf_response, uint16_t& len_response ) const;
  bool GetAttr7( uint8_t* buf_response, uint16_t& len_response ) const;
  bool GetAttr8( uint8_t* buf_response, uint16_t& len_response ) const;
  bool GetAttr9( uint8_t* buf_response, uint16_t& len_response ) const;
  bool GetAttr10( uint8_t* buf_response, uint16_t& len_response ) const;
  bool GetAttr11( uint8_t* buf_response, uint16_t& len_response ) const;
  bool GetAttr12( uint8_t* buf_response, uint16_t& len_response ) const;
  bool GetAttr13( uint8_t* buf_response, uint16_t& len_response ) const;
  
  uint8_t SetAttr3( uint8_t* buf_request );
  uint8_t SetAttr4( uint8_t* buf_request );
  uint8_t SetAttr6( uint8_t* buf_request );
  uint8_t SetAttr7( uint8_t* buf_request );
  uint8_t SetAttr8( uint8_t* buf_request );
  uint8_t SetAttr9( uint8_t* buf_request );
  uint8_t SetAttr13( uint8_t* buf_request );
  /*
  void ActMeth1();
  void ActMeth2();
  void ActMeth3();
  void ActMeth4();
  void ActMeth5();
  */

  void UpdateUnitCharge( chargeTableElementType* elements );
  void ActivatePassiveUnitCharge( s32 data );
  void Collect( s32 data );
  s32 UpdateTotalAmountRemaining( s32 valueToAdd );
  s32 SetTotalAmountRemaining( s32 newValue );
      
  /* Functions for internal work */
  s32 UpdateTotalAmountPaid( s32 collectionValue );
  void UpdateLastCollectionTime();
  void UpdateLastCollectionAmount( s32 sum );
  s32 ReduceTotalAmountRemaining( s32 sum );
  u32 CalcPeriodPassed() const;
  void ExecuteConsumptionBasedCollection();
  void ExecuteTimeBasedCollection();
  s16 GetCurrentChargePerUnit() const;
//  u32 GetUnitsConsumedFromLastCollection() const;
  
  PaymentChargeCfg* const       chargeCfg;
  const ftPaymentCharge* const  ftFile;
  
  PaymentChargeDynamicValues    currValues;
  
  u64                           lastValue[MAX_TARIFFS]; // value from the register that was processed a "period" ago
    
  bool                          isLinkedAccountActive;
  
  bool                          newCollection;          // set in TRUE by the Charge if the current collection must be processed 
  s32                           sumToCollect;           // amount of the current collection
};

/*********************************************/
/********** Token Gateway's Interface ********/
/*********************************************/
class PaymentTokenGatewayClass : public COSEMInterfaceClassAbstract{
public:
  PaymentTokenGatewayClass( const LOGICAL_NAME* const _ln, const ftPaymentTokenGateway* const _ftFile ); 
  
  const LOGICAL_NAME* const ln;
  
  bool Get( uint8_t attrID, uint8_t* buf_request, uint8_t* buf_response, uint16_t& len_response );
  void Action( uint8_t attrID, uint8_t* buf_request, uint8_t* buf_response, uint16_t& len_response );
  
  void Init();
  void IdleMinute();
  
  /* Functions for communication between Payment classes */
  bool GetPermissionToReconnectRelay() const;
//  bool GetNewTokenTopUp() const;
//  s32 GetTopUpSum() const;
  u32 GetTokenID() const;
  const BYTE* GetActiveTransactionID() const;
  u32 GetExpiresTimeSec() const;
  u8 GetExpiresTimeStatus() const;
  void ConfirmReceivedToken();
  void RefuseReceivedToken();
  
  const u8 inTokenType = 0;
  
  enum class commonFieldPosInToken{
    tag                 = 0,
    len,
    type                = 2,
    rxInvocCounter      = 3,
    subtype             = 7,
    tokenID             = 8,
    expiresTime         = 12,
    expiresTimeStatus   = 16,
    transactionID       = 17    /* order ID */
  };
  
  enum class specificFieldPosStartPaidToken{
    amount              = 33,
    currency            = 37,
    ek                  = 48,
    ak                  = 72
  };
  
  enum class specificFieldPosTopUpToken{
    amount              = 33,
    currency            = 37
  };
  
  enum class specificFieldPosStartNonPaidToken{
    ek                  = 33,
    ak                  = 57
  };
  
  enum class inTokenLen{
      startPaid         = 106,
      topUp             = 58,
      stopPaid          = 43,
      startNonPaid      = 91,
      stopNonPaid       = 43
  };  
  
private:      
  bool GetAttr2( uint8_t* buf_response, uint16_t& len_response ) const;
  bool GetAttr3( uint8_t* buf_response, uint16_t& len_response ) const;
  bool GetAttr4( uint8_t* buf_response, uint16_t& len_response ) const;       // Now we don't have token_description
  bool GetAttr5( uint8_t* buf_response, uint16_t& len_response ) const;
  bool GetAttr6( uint8_t* buf_response, uint16_t& len_response ) const;
  bool GetAttrTokenID( uint8_t* buf_response, uint16_t& len_response ) const; 
  /*  
  void ActMeth1() = 0;
  */  
    
  eT_tokenStatusCode Enter( uint8_t* rxToken );
  
  /* Functions for internal work */
  eT_tokenStatusCode CheckReceivedToken( uint8_t* tokenRx ) const;
  bool CheckFormatReceivedToken( uint8_t* tokenRx ) const;
  bool CheckLenReceivedToken( uint8_t lenRxToken, uint8_t rxTokenSubtype ) const;
  eT_tokenStatusCode CheckCommonFieldsReceivedToken( uint8_t* tokenRx ) const;
  bool CheckReceivedTokenSubtype( u8 rxSubtype ) const;
  bool CheckReceivedTokenID( u32 rxTID, u8 rxTokenSubtype ) const;
  bool CheckForDuplicatesReceivedTokenID( u32 rxTID ) const;
  bool CheckReceivedExpiresTime( u32 rxExpiresTimeSec, u8 rxExpiresTimeStatus, u8 rxTokenSubtype ) const;
  bool CheckReceivedOrderID( uint8_t* rxTransactionID, u8 rxTokenSubtype ) const;
  bool CheckSpecificFieldsReceivedToken( uint8_t* tokenRx ) const;
  
  void UpdateTokenFormatFields( uint8_t* tokenRx );
  void UpdateTokenSubtype( inTokenSubtype newTokenSubtype );
  void IncrementNextReceivedTokenIndex();
  void UpdateTokenID( u32 newTokenID );
  void UpdateExpiresTime( u32 newExpiresTime, u8 newExpiresTimeStatus );
  void UpdateTimeOfStart( u32 newTimeOfStartSec, u8 newTimeOfStartStatus );
  void UpdateTransactionID(uint8_t* newTransactionID );
  void ResetActiveTransactionID();
  
  bool HasExpiresTimeCome() const;
  void ProcessExpiresTime();
  
  void ProcessToken( uint8_t* tokenRx );
  
  const ftPaymentTokenGateway* const    ftFile;
  
  PaymentTokenGatewayDynamicValues      currValues;
  PaymentTokenFormat                    lastToken;
  
//  bool                                  newTokenTopUp;
//  s32                                   topUpSum;
};

/*********************************************/
/************* Account's Interface ***********/
/*********************************************/
class PaymentAccountClass : public COSEMInterfaceClassAbstract{
public:
  PaymentAccountClass( const LOGICAL_NAME* const        _ln,
                      PaymentAccountCfg* const          _cfg,
                      PaymentCreditClass* const         _creditList[],
                      PaymentChargeClass* const         _chargeList[],
                      PaymentTokenGatewayClass* const   _tokenGateway,
                      const ftPaymentAccount* const     _ftFile );
  
  const LOGICAL_NAME* const     ln;
  
  bool Get( uint8_t attrID, uint8_t* buf_request, uint8_t* buf_response, uint16_t& len_response );
  uint8_t Set( uint8_t attrID, uint8_t* buf_request );
  //     void Action( uint8_t attrID, uint8_t* buf_request, uint8_t* buf_response, uint16_t& len_response );
  
  void Init();
  void IdleSecond();
  void IdleMinute();
  
  /* Functions for communication with StartStopService class */
  bool IsAccountActive() const;
//  eT_tokenStatusCode RedirectToTokenGatewayEnter( uint8_t* buf_request ) const;
  s32 GetSumOfAllCurrentCreditAmount() const;
  TDateTime* GetAccountActivationTime() const;
  TDateTime* GetAccountClosureTime() const;
  s8 GetCurrencyScale() const;
  s32 GetSumOfAllChargeTotalAmountPaid() const;
  
  void ActivateAccount( s32 data  = 0 );
  void CloseAccount( s32 data = 0 );
  void ResetAccount( s32 data = 0 );
  void TopUpCredits( s32 topUpSum );   /* This function will be invoked by TokenGateway Object with StartPaid token and TopUp token */
  
private:      
  bool GetAttr2( uint8_t* buf_response, uint16_t& len_response ) const;
  bool GetAttr3( uint8_t* buf_response, uint16_t& len_response ) const;
  bool GetAttr4( uint8_t* buf_response, uint16_t& len_response ) const;
  bool GetAttr5( uint8_t* buf_response, uint16_t& len_response ) const;
  bool GetAttr6( uint8_t* buf_response, uint16_t& len_response ) const;
  bool GetAttr7( uint8_t* buf_response, uint16_t& len_response ) const;
  bool GetAttr8( uint8_t* buf_response, uint16_t& len_response ) const;
  bool GetAttr9( uint8_t* buf_response, uint16_t& len_response ) const;
  bool GetAttr10( uint8_t* buf_response, uint16_t& len_response ) const;
  bool GetAttr11( uint8_t* buf_response, uint16_t& len_response ) const;
  bool GetAttr12( uint8_t* buf_response, uint16_t& len_response ) const;
  bool GetAttr13( uint8_t* buf_response, uint16_t& len_response ) const;
  bool GetAttr14( uint8_t* buf_response, uint16_t& len_response ) const;
  bool GetAttr15( uint8_t* buf_response, uint16_t& len_response ) const;
  bool GetAttr16( uint8_t* buf_response, uint16_t& len_response ) const;
  bool GetAttr17( uint8_t* buf_response, uint16_t& len_response ) const;
  bool GetAttr18( uint8_t* buf_response, uint16_t& len_response ) const;
  bool GetAttr19( uint8_t* buf_response, uint16_t& len_response ) const;
  
  uint8_t SetAttr7( uint8_t* buf_request );
  uint8_t SetAttr12( uint8_t* buf_request );
  uint8_t SetAttr13( uint8_t* buf_request );
  uint8_t SetAttr14( uint8_t* buf_request );
  uint8_t SetAttr15( uint8_t* buf_request );
  uint8_t SetAttr18( uint8_t* buf_request );
  uint8_t SetAttr19( uint8_t* buf_request );
  /*
  void ActMeth1( s32 data = 0 );
  void ActMeth2( s32 data = 0 );
  void ActMeth3( s32 data = 0);
  */
  
  /* Functions for internal work */
  void ExecuteCollection( u8 chargeIndex );
  bool CheckCreditChargeConfiguration( u8 chargeIndex );        /* The functions returns: true if charge can collect from credit in_use; false if charge cann't collect from credit in_use. */
  void DistributeTopUpSumBetweenCredits( s32 topUpSum );
  void DistributeTopUpSumWithoutRestrictions( s32 topUpSum );
  void DistributeTopUpSumAccordingToProportion( s32 topUpSum );
  bool InvokeHighestPriorityCreditToInUse();
  void UpdateCurrentCreditStatus();
  void UpdateAvailableCredit();
  void UpdateAmountToClear();
  void UpdateAggregatedDebt();
  void UpdateLowCreditThreshold();
  void UpdateNextCreditAvailableThreshold();
  void ManageCreditsStatuses();
  void ActivateLinkedCharges() const;
  void CloseLinkedCharges() const;
  u8 FindIndexOfNextPriorityCredit() const;
  
  PaymentAccountCfg* const      accountCfg;
  PaymentCreditClass*           creditList[MAX_OBJECTS_IN_CREDIT_REF_LIST];
  PaymentChargeClass*           chargeList[MAX_OBJECTS_IN_CHARGE_REF_LIST];
  PaymentTokenGatewayClass*     tokenGateway;
  const ftPaymentAccount* const ftFile;
  
  PaymentAccountDynamicValues   currValues;
  
  uint8_t                       lenCreditList;
  uint8_t                       lenChargeList;
  uint8_t                       lenCreditChargeCfgList;
  uint8_t                       lenTokenGatewayCfgList;         
};

/************************************************************************************************/
/*************************** Interfaces of Assist classes ***************************************/
/************************************************************************************************/

class OutTokenClass : public COSEMInterfaceClassAbstract{
public:
  enum OutTokenAttributes{
    ValueAttr           = 2
  };
  
  OutTokenClass( const LOGICAL_NAME* const _ln );
  
  const u8 outTokenType = 1;
  
  enum class outTokenSubtype{
    startPaidResponse    = 1,
    topUpResponse        = 2,
    stopPaidResponse     = 3,
    startNonPaidResponse = 4,
    stopNonPaidResponse  = 5,
    none
  };
  
  enum class outTokenLen{
    startPaidResponse    = 76,
    topUpResponse        = 76,
    stopPaidResponse     = 76,
    startNonPaidResponse = 68,
    stopNonPaidResponse  = 68,
    max                  = stopPaidResponse
  };

  enum class commonFieldPosOutToken {
    type                = 0,
    txInvocCounter      = 1,
    subtype             = 5,
    tokenID             = 6,
    transactionID       = 10,    /* order ID */
    startTime           = 26,
    startTimeStatus     = 30,
    tokenTime           = 31,
    tokenTimeStatus     = 35,
    activeEnergy        = 36,
    usedEnergy          = 44,
    states              = 48,
    alarms              = 52
  };
  
  enum class specificPaidFieldPosOutToken {
    availableCredit     = 56,
    usedCredit          = 60                 
  };

  bool Get( uint8_t attrID, uint8_t* buf_request, uint8_t* buf_response, uint16_t& len_response );
  
  void UpdateValue( inTokenSubtype inSubtype, u32 tokenID, const BYTE* transactionID, u32 startTime, u8 startTimeStatus ); 
    
private:
  bool GetAttr2( uint8_t* buf_response, uint16_t& len_response ) const;
  
  const LOGICAL_NAME* const ln;
};

class ActiveTransactionIDClass : public COSEMInterfaceClassAbstract{
public:
  enum ActiveTransactionIDAttributes{
    ValueAttr           = 2  
  };
  
  ActiveTransactionIDClass( const LOGICAL_NAME* const _ln );
  
  const LOGICAL_NAME* const ln;
  
  bool Get( uint8_t attrID, uint8_t* buf_request, uint8_t* buf_response, uint16_t& len_response );
  
private:
  bool GetAttr2( uint8_t* buf_response, uint16_t& len_response ) const;
};

class TopUpsSumClass : public COSEMInterfaceClassAbstract{
public:
  enum StartTopUpSumClassAttributes{
    ValueAttr            = 2,
    ScalerUnitAttr       = 3       
  };
  
  TopUpsSumClass( const LOGICAL_NAME* const _ln );
  
  const LOGICAL_NAME* const ln;
  
  bool Get( uint8_t attrID, uint8_t* buf_request, uint8_t* buf_response, uint16_t& len_response );
  
private:
  bool GetAttr2( uint8_t* buf_response, uint16_t& len_response ) const;
  bool GetAttr3( uint8_t* buf_response, uint16_t& len_response ) const;
};

class TotalAmountPaidClass : public COSEMInterfaceClassAbstract{
public:
  enum TotalAmountPaidAttributes{
    ValueAttr            = 2,
    ScalerUnitAttr       = 3       
  };
  
  TotalAmountPaidClass( const LOGICAL_NAME* const _ln );
  
  const LOGICAL_NAME* const ln;
  
  bool Get( uint8_t attrID, uint8_t* buf_request, uint8_t* buf_response, uint16_t& len_response );
  
private:
  bool GetAttr2( uint8_t* buf_response, uint16_t& len_response ) const;
  bool GetAttr3( uint8_t* buf_response, uint16_t& len_response ) const;
};

class ConsumedKWhFromStartClass : public COSEMInterfaceClassAbstract{
public:
  enum TotalAmountPaidAttributes{
    ValueAttr            = 2,
    ScalerUnitAttr       = 3       
  };
  
  ConsumedKWhFromStartClass( const LOGICAL_NAME* const _ln );
  
  const LOGICAL_NAME* const ln;
  
  void UpdateKWhWhenStart();
  u32 GetConsumedKWh() const;
  
  bool Get( uint8_t attrID, uint8_t* buf_request, uint8_t* buf_response, uint16_t& len_response );
  
  void Init();
  
private:
  bool GetAttr2( uint8_t* buf_response, uint16_t& len_response ) const;
  bool GetAttr3( uint8_t* buf_response, uint16_t& len_response ) const;
  
  u64 kWhWhenStart; /* Value of A Rg when was start */
};

class TokenIDClass : public COSEMInterfaceClassAbstract{
public:
  enum TokenIDAttributes{
    ValueAttr            = 2      
  };
  
  TokenIDClass( const LOGICAL_NAME* const _ln );
  
  const LOGICAL_NAME* const ln;
  
  bool Get( uint8_t attrID, uint8_t* buf_request, uint8_t* buf_response, uint16_t& len_response );
  
private:
  bool GetAttr2( uint8_t* buf_response, uint16_t& len_response ) const;
};

class ExpiresTimeClass : public COSEMInterfaceClassAbstract{
public:
  enum ExpiresTimeAttributes{
    ValueAttr            = 2      
  };
  
  ExpiresTimeClass( const LOGICAL_NAME* const _ln );
  
  const LOGICAL_NAME* const ln;
  
  bool Get( uint8_t attrID, uint8_t* buf_request, uint8_t* buf_response, uint16_t& len_response );
  
private:
  bool GetAttr2( uint8_t* buf_response, uint16_t& len_response ) const;
};

/***********************************************************/
extern PaymentTokenGatewayClass TokenGatewayForImportAccount;
/***********************************************************/

#endif // _PAYMENT_