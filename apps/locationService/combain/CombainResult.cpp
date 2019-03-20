#include "CombainResult.h"


CombainResult::CombainResult(ma_combainLocation_Result_t type)
    : type(type)
{}


ma_combainLocation_Result_t CombainResult::getType(void) const
{
    return this->type;
}


CombainSuccessResponse::CombainSuccessResponse(
    double latitude, double longitude, double accuracyInMeters)
    : CombainResult(MA_COMBAINLOCATION_RESULT_SUCCESS),
      latitude(latitude),
      longitude(longitude),
      accuracyInMeters(accuracyInMeters)
{}


CombainErrorResponse::CombainErrorResponse(
    uint16_t code, const std::string &message, std::initializer_list<CombainError> errors)
    : CombainResult(MA_COMBAINLOCATION_RESULT_ERROR),
      code(code),
      message(message),
      errors(errors)
{}


CombainResponseParseFailure::CombainResponseParseFailure(const std::string &unparsed)
    : CombainResult(MA_COMBAINLOCATION_RESULT_RESPONSE_PARSE_FAILURE),
      unparsed(unparsed)
{}


CombainCommunicationFailure::CombainCommunicationFailure(void)
    : CombainResult(MA_COMBAINLOCATION_RESULT_COMMUNICATION_FAILURE)
{}
