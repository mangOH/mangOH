#ifndef COMBAIN_RESULT_H
#define COMBAIN_RESULT_H

#include "legato.h"
#include "interfaces.h"

#include <initializer_list>
#include <list>
#include <string>


class CombainResult
{
public:
    explicit CombainResult(ma_combainLocation_Result_t type);
    ma_combainLocation_Result_t getType(void) const;
private:
    ma_combainLocation_Result_t type;
};


struct CombainSuccessResponse : public CombainResult
{
    CombainSuccessResponse(double latitude, double longitude, double accuracyInMeters);

    double latitude;
    double longitude;
    double accuracyInMeters;
};


struct CombainError
{
    std::string domain;
    std::string reason;
    std::string message;
};

struct CombainErrorResponse : public CombainResult
{
    CombainErrorResponse(
        uint16_t code, const std::string &message, std::initializer_list<CombainError> errors);
    uint16_t code;
    std::string message;
    std::list<CombainError> errors;
};

struct CombainResponseParseFailure : public CombainResult
{
    explicit CombainResponseParseFailure(const std::string& unparsed);
    std::string unparsed;
};

struct CombainCommunicationFailure : public CombainResult
{
    CombainCommunicationFailure(void);
};


#endif // COMBAIN_RESULT_H
