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
*/
//==============================================================================

#include <BeastConfig.h>
#include <casinocoin/protocol/STTx.h>
#include <casinocoin/protocol/HashPrefix.h>
#include <casinocoin/protocol/JsonFields.h>
#include <casinocoin/protocol/PublicKey.h>
#include <casinocoin/protocol/Protocol.h>
#include <casinocoin/protocol/Sign.h>
#include <casinocoin/protocol/STAccount.h>
#include <casinocoin/protocol/STArray.h>
#include <casinocoin/protocol/TxFlags.h>
#include <casinocoin/protocol/types.h>
#include <casinocoin/core/Config.h>
#include <casinocoin/basics/contract.h>
#include <casinocoin/basics/Log.h>
#include <casinocoin/basics/StringUtilities.h>
#include <casinocoin/json/to_string.h>
#include <boost/format.hpp>
#include <array>
#include <memory>
#include <type_traits>
#include <utility>

namespace casinocoin {

static
auto getTxFormat (TxType type)
{
    auto format = TxFormats::getInstance().findByType (type);

    if (format == nullptr)
    {
        Throw<std::runtime_error> (
            "Invalid transaction type " +
            std::to_string (
                static_cast<std::underlying_type_t<TxType>>(type)));
    }

    return format;
}

STTx::STTx (STObject&& object)
    : STObject (std::move (object))
{
    tx_type_ = static_cast <TxType> (getFieldU16 (sfTransactionType));

    if (!setType (getTxFormat (tx_type_)->elements))
        Throw<std::runtime_error> ("transaction not valid");

    tid_ = getHash(HashPrefix::transactionID);
}

STTx::STTx (SerialIter& sit)
    : STObject (sfTransaction)
{
    int length = sit.getBytesLeft ();

    if ((length < Protocol::txMinSizeBytes) || (length > Protocol::txMaxSizeBytes))
        Throw<std::runtime_error> ("Transaction length invalid");

    set (sit);
    tx_type_ = static_cast<TxType> (getFieldU16 (sfTransactionType));

    if (!setType (getTxFormat (tx_type_)->elements))
        Throw<std::runtime_error> ("transaction not valid");

    tid_ = getHash(HashPrefix::transactionID);
}

STTx::STTx (
        TxType type,
        std::function<void(STObject&)> assembler)
    : STObject (sfTransaction)
{
    auto format = getTxFormat (type);

    set (format->elements);
    setFieldU16 (sfTransactionType, format->getType ());

    assembler (*this);

    tx_type_ = static_cast<TxType>(getFieldU16 (sfTransactionType));

    if (tx_type_ != type)
        LogicError ("Transaction type was mutated during assembly");

    tid_ = getHash(HashPrefix::transactionID);
}

std::string
STTx::getFullText () const
{
    std::string ret = "\"";
    ret += to_string (getTransactionID ());
    ret += "\" = {";
    ret += STObject::getFullText ();
    ret += "}";
    return ret;
}

boost::container::flat_set<AccountID>
STTx::getMentionedAccounts () const
{
    boost::container::flat_set<AccountID> list;

    for (auto const& it : *this)
    {
        if (auto sa = dynamic_cast<STAccount const*> (&it))
        {
            assert(! sa->isDefault());
            if (! sa->isDefault())
                list.insert(sa->value());
        }
        else if (auto sa = dynamic_cast<STAmount const*> (&it))
        {
            auto const& issuer = sa->getIssuer ();
            if (! isCSC (issuer))
                list.insert(issuer);
        }
    }

    return list;
}

static Blob getSigningData (STTx const& that)
{
    Serializer s;
    s.add32 (HashPrefix::txSign);
    that.addWithoutSigningFields (s);
    return s.getData();
}

uint256
STTx::getSigningHash () const
{
    return STObject::getSigningHash (HashPrefix::txSign);
}

Blob STTx::getSignature () const
{
    try
    {
        return getFieldVL (sfTxnSignature);
    }
    catch (std::exception const&)
    {
        return Blob ();
    }
}

void STTx::sign (
    PublicKey const& publicKey,
    SecretKey const& secretKey)
{
    auto const data = getSigningData (*this);

    auto const sig = casinocoin::sign (
        publicKey,
        secretKey,
        makeSlice(data));

    setFieldVL (sfTxnSignature, sig);
    tid_ = getHash(HashPrefix::transactionID);
}

std::pair<bool, std::string> STTx::checkSign(bool allowMultiSign) const
{
    std::pair<bool, std::string> ret {false, ""};
    try
    {
        if (allowMultiSign)
        {
            // Determine whether we're single- or multi-signing by looking
            // at the SigningPubKey.  It it's empty we must be
            // multi-signing.  Otherwise we're single-signing.
            Blob const& signingPubKey = getFieldVL (sfSigningPubKey);
            ret = signingPubKey.empty () ?
                checkMultiSign () : checkSingleSign ();
        }
        else
        {
            ret = checkSingleSign ();
        }
    }
    catch (std::exception const&)
    {
        ret = {false, "Internal signature check failure."};
    }
    return ret;
}

Json::Value STTx::getJson (int) const
{
    Json::Value ret = STObject::getJson (0);
    ret[jss::hash] = to_string (getTransactionID ());
    return ret;
}

Json::Value STTx::getJson (int options, bool binary) const
{
    if (binary)
    {
        Json::Value ret;
        Serializer s = STObject::getSerializer ();
        ret[jss::tx] = strHex (s.peekData ());
        ret[jss::hash] = to_string (getTransactionID ());
        return ret;
    }
    return getJson(options);
}

std::string const&
STTx::getMetaSQLInsertReplaceHeader ()
{
    static std::string const sql = "INSERT OR REPLACE INTO Transactions "
        "(TransID, TransType, FromAcct, FromSeq, LedgerSeq, Status, RawTxn, TxnMeta)"
        " VALUES ";

    return sql;
}

std::string STTx::getMetaSQL (std::uint32_t inLedger,
                                               std::string const& escapedMetaData) const
{
    Serializer s;
    add (s);
    return getMetaSQL (s, inLedger, TXN_SQL_VALIDATED, escapedMetaData);
}

// VFALCO This could be a free function elsewhere
std::string
STTx::getMetaSQL (Serializer rawTxn,
    std::uint32_t inLedger, char status, std::string const& escapedMetaData) const
{
    static boost::format bfTrans ("('%s', '%s', '%s', '%d', '%d', '%c', %s, %s)");
    std::string rTxn = sqlEscape (rawTxn.peekData ());

    auto format = TxFormats::getInstance().findByType (tx_type_);
    assert (format != nullptr);

    return str (boost::format (bfTrans)
                % to_string (getTransactionID ()) % format->getName ()
                % toBase58(getAccountID(sfAccount))
                % getSequence () % inLedger % status % rTxn % escapedMetaData);
}

std::pair<bool, std::string> STTx::checkSingleSign () const
{
    // We don't allow both a non-empty sfSigningPubKey and an sfSigners.
    // That would allow the transaction to be signed two ways.  So if both
    // fields are present the signature is invalid.
    if (isFieldPresent (sfSigners))
        return {false, "Cannot both single- and multi-sign."};

    bool validSig = false;
    try
    {
        bool const fullyCanonical = (getFlags() & tfFullyCanonicalSig);
        auto const spk = getFieldVL (sfSigningPubKey);

        if (publicKeyType (makeSlice(spk)))
        {
            Blob const signature = getFieldVL (sfTxnSignature);
            Blob const data = getSigningData (*this);

            validSig = verify (
                PublicKey (makeSlice(spk)),
                makeSlice(data),
                makeSlice(signature),
                fullyCanonical);
        }
    }
    catch (std::exception const&)
    {
        // Assume it was a signature failure.
        validSig = false;
    }
    if (validSig == false)
        return {false, "Invalid signature."};

    return {true, ""};
}

std::pair<bool, std::string> STTx::checkMultiSign () const
{
    // Make sure the MultiSigners are present.  Otherwise they are not
    // attempting multi-signing and we just have a bad SigningPubKey.
    if (!isFieldPresent (sfSigners))
        return {false, "Empty SigningPubKey."};

    // We don't allow both an sfSigners and an sfTxnSignature.  Both fields
    // being present would indicate that the transaction is signed both ways.
    if (isFieldPresent (sfTxnSignature))
        return {false, "Cannot both single- and multi-sign."};

    STArray const& signers {getFieldArray (sfSigners)};

    // There are well known bounds that the number of signers must be within.
    if (signers.size() < minMultiSigners || signers.size() > maxMultiSigners)
        return {false, "Invalid Signers array size."};

    // We can ease the computational load inside the loop a bit by
    // pre-constructing part of the data that we hash.  Fill a Serializer
    // with the stuff that stays constant from signature to signature.
    Serializer const dataStart {startMultiSigningData (*this)};

    // We also use the sfAccount field inside the loop.  Get it once.
    auto const txnAccountID = getAccountID (sfAccount);

    // Determine whether signatures must be full canonical.
    bool const fullyCanonical = (getFlags() & tfFullyCanonicalSig);

    // Signers must be in sorted order by AccountID.
    AccountID lastAccountID (beast::zero);

    for (auto const& signer : signers)
    {
        auto const accountID = signer.getAccountID (sfAccount);

        // The account owner may not multisign for themselves.
        if (accountID == txnAccountID)
            return {false, "Invalid multisigner."};

        // No duplicate signers allowed.
        if (lastAccountID == accountID)
            return {false, "Duplicate Signers not allowed."};

        // Accounts must be in order by account ID.  No duplicates allowed.
        if (lastAccountID > accountID)
            return {false, "Unsorted Signers array."};

        // The next signature must be greater than this one.
        lastAccountID = accountID;

        // Verify the signature.
        bool validSig = false;
        try
        {
            Serializer s = dataStart;
            finishMultiSigningData (accountID, s);

            auto spk = signer.getFieldVL (sfSigningPubKey);

            if (publicKeyType (makeSlice(spk)))
            {
                Blob const signature =
                    signer.getFieldVL (sfTxnSignature);

                validSig = verify (
                    PublicKey (makeSlice(spk)),
                    s.slice(),
                    makeSlice(signature),
                    fullyCanonical);
            }
        }
        catch (std::exception const&)
        {
            // We assume any problem lies with the signature.
            validSig = false;
        }
        if (!validSig)
            return {false, std::string("Invalid signature on account ") +
                toBase58(accountID)  + "."};
    }

    // All signatures verified.
    return {true, ""};
}

//------------------------------------------------------------------------------

static
bool
isMemoOkay (STObject const& st, std::string& reason, const Config& config)
{
    if (!st.isFieldPresent (sfMemos))
        return true;

    auto const& memos = st.getFieldArray (sfMemos);

    // The number 2048 is a preallocation hint, not a hard limit
    // to avoid allocate/copy/free's
    Serializer s (2048);
    memos.add (s);

    if (s.getDataLength () > config.MAX_MEMO_SIZE)
    {
        reason = "Max Memo size exceded, max memo = "+std::to_string(config.MAX_MEMO_SIZE);
        return false;
    }

    for (auto const& memo : memos)
    {
        auto memoObj = dynamic_cast <STObject const*> (&memo);

        if (!memoObj || (memoObj->getFName() != sfMemo))
        {
            reason = "A memo array may contain only Memo objects.";
            return false;
        }

        for (auto const& memoElement : *memoObj)
        {
            auto const& name = memoElement.getFName();

            if (name != sfMemoType &&
                name != sfMemoData &&
                name != sfMemoFormat)
            {
                reason = "A memo may contain only MemoType, MemoData or "
                         "MemoFormat fields.";
                return false;
            }

            // The raw data is stored as hex-octets, which we want to decode.
            auto data = strUnHex (memoElement.getText ());

            if (!data.second)
            {
                reason = "The MemoType, MemoData and MemoFormat fields may "
                         "only contain hex-encoded data.";
                return false;
            }

            if (name == sfMemoData)
                continue;

            // The only allowed characters for MemoType and MemoFormat are the
            // characters allowed in URLs per RFC 3986: alphanumerics and the
            // following symbols: -._~:/?#[]@!$&'()*+,;=%
            static std::array<char, 256> const allowedSymbols = []
            {
                std::array<char, 256> a;
                a.fill(0);

                std::string symbols (
                    "0123456789"
                    "-._~:/?#[]@!$&'()*+,;=%"
                    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                    "abcdefghijklmnopqrstuvwxyz");

                for(char c : symbols)
                    a[c] = 1;
                return a;
            }();

            for (auto c : data.first)
            {
                if (!allowedSymbols[c])
                {
                    reason = "The MemoType and MemoFormat fields may only "
                             "contain characters that are allowed in URLs "
                             "under RFC 3986.";
                    return false;
                }
            }
        }
    }

    return true;
}

// Ensure all account fields are 160-bits
static
bool
isAccountFieldOkay (STObject const& st)
{
    for (int i = 0; i < st.getCount(); ++i)
    {
        auto t = dynamic_cast<STAccount const*>(st.peekAtPIndex (i));
        if (t && t->isDefault ())
            return false;
    }

    return true;
}

bool passesLocalChecks (STObject const& st, std::string& reason, const Config& config)
{
    if (!isMemoOkay (st, reason, config))
        return false;

    if (!isAccountFieldOkay (st))
    {
        reason = "An account field is invalid.";
        return false;
    }

    if (isPseudoTx(st))
    {
        reason = "Cannot submit pseudo transactions.";
        return false;
    }
    return true;
}

std::shared_ptr<STTx const>
sterilize (STTx const& stx)
{
    Serializer s;
    stx.add(s);
    SerialIter sit(s.slice());
    return std::make_shared<STTx const>(std::ref(sit));
}

bool
isPseudoTx(STObject const& tx)
{
    auto t = tx[~sfTransactionType];
    if (!t)
        return false;
    auto tt = static_cast<TxType>(*t);
    return tt == ttAMENDMENT || tt == ttFEE || tt == ttCONFIG;
}

} // casinocoin
