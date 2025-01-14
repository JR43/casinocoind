//------------------------------------------------------------------------------
/*
    This file is part of rippled: https://github.com/ripple/rippled
    Copyright (c) 2012, 2013 Ripple Labs Inc.

    Permission to use, copy, modify, and/or distribute this software for any
    purpose  with  or without fee is hereby granted, provided that the above
    copyright notice and this permission notice appear in all copies.

    THE  SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
    WITH  REGARD  TO  THIS  SOFTWARE  INCLUDING  ALL  IMPLIED  WARRANTIES  OF
    MERCHANTABILITY  AND  FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
    ANY  SPECIAL ,  DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
    WHATSOEVER  RESULTING  FROM  LOSS  OF USE, DATA OR PROFITS, WHETHER IN AN
    ACTION  OF  CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
    OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/
//==============================================================================

//==============================================================================
/*
    2017-06-30  ajochems        Refactored for casinocoin
    2018-01-11  jrojek          added KYC
*/
//==============================================================================

#ifndef CASINOCOIN_PROTOCOL_FEATURE_H_INCLUDED
#define CASINOCOIN_PROTOCOL_FEATURE_H_INCLUDED

#include <casinocoin/basics/base_uint.h>
#include <string>

namespace casinocoin {

/** Convert feature description to feature id. */
/** @{ */
uint256
feature (std::string const& name);

uint256
feature (const char* name);
/** @} */

extern uint256 const featureMultiSign;
extern uint256 const featureTickets;
extern uint256 const featureTrustSetAuth;
extern uint256 const featureFeeEscalation;
extern uint256 const featureOwnerPaysFee;
extern uint256 const featureCompareFlowV1V2;
extern uint256 const featureSHAMapV2;
extern uint256 const featurePayChan;
extern uint256 const featureFlow;
extern uint256 const featureCompareTakerFlowCross;
extern uint256 const featureFlowCross;
extern uint256 const featureCryptoConditions;
extern uint256 const featureTickSize;
extern uint256 const fix1368;
extern uint256 const featureEscrow;
extern uint256 const featureCryptoConditionsSuite;
extern uint256 const fix1373;
extern uint256 const featureEnforceInvariants;
extern uint256 const featureKYC;
extern uint256 const featureKYCIPTracking;
extern uint256 const featureConfigObject;
extern uint256 const featureWLT;

} // casinocoin

#endif
