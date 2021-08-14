//------------------------------------------------------------------------------
/*
  This file is part of rippled: https://github.com/ripple/rippled
  Copyright (c) 2021 Ripple Labs Inc.

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

#include <ripple/app/tx/impl/details/NFTokenUtils.h>
#include <ripple/basics/random.h>
#include <ripple/protocol/Feature.h>
#include <ripple/protocol/jss.h>
#include <test/jtx.h>

#include <random>

namespace ripple {

class NFToken_test : public beast::unit_test::suite
{
    // Helper function that returns the owner count of an account root.
    static std::uint32_t
    ownerCount(test::jtx::Env const& env, test::jtx::Account const& acct)
    {
        std::uint32_t ret{0};
        if (auto const sleAcct = env.le(acct))
            ret = sleAcct->at(sfOwnerCount);
        return ret;
    }

    // Helper function that returns the number of NFTs minted by an issuer.
    static std::uint32_t
    mintedCount(test::jtx::Env const& env, test::jtx::Account const& issuer)
    {
        std::uint32_t ret{0};
        if (auto const sleIssuer = env.le(issuer))
            ret = sleIssuer->at(~sfMintedTokens).value_or(0);
        return ret;
    }

    // Helper function that returns the number of an issuer's burned NFTs.
    static std::uint32_t
    burnedCount(test::jtx::Env const& env, test::jtx::Account const& issuer)
    {
        std::uint32_t ret{0};
        if (auto const sleIssuer = env.le(issuer))
            ret = sleIssuer->at(~sfBurnedTokens).value_or(0);
        return ret;
    }

    // Helper function returns the close time of the parent ledger.
    std::uint32_t
    lastClose(test::jtx::Env& env)
    {
        return env.current()->info().parentCloseTime.time_since_epoch().count();
    }

    void
    testEnabled(FeatureBitset features)
    {
        testcase("Enabled");

        using namespace test::jtx;
        {
            // If the NFT amendment is not enabled, you should not be able
            // to create or burn NFTs.
            Env env{*this, features - featureNonFungibleTokensV1};
            Account const& master = env.master;

            BEAST_EXPECT(ownerCount(env, master) == 0);
            BEAST_EXPECT(mintedCount(env, master) == 0);
            BEAST_EXPECT(burnedCount(env, master) == 0);

            uint256 const nftId{token::getNextID(env, master, 0u)};
            env(token::mint(master, 0u), ter(temDISABLED));
            env.close();
            BEAST_EXPECT(ownerCount(env, master) == 0);
            BEAST_EXPECT(mintedCount(env, master) == 0);
            BEAST_EXPECT(burnedCount(env, master) == 0);

            env(token::burn(master, nftId), ter(temDISABLED));
            env.close();
            BEAST_EXPECT(ownerCount(env, master) == 0);
            BEAST_EXPECT(mintedCount(env, master) == 0);
            BEAST_EXPECT(burnedCount(env, master) == 0);

            uint256 const offerIndex =
                keylet::nftoffer(master, env.seq(master)).key;
            env(token::createOffer(master, nftId, XRP(10)), ter(temDISABLED));
            env.close();
            BEAST_EXPECT(ownerCount(env, master) == 0);
            BEAST_EXPECT(mintedCount(env, master) == 0);
            BEAST_EXPECT(burnedCount(env, master) == 0);

            env(token::cancelOffer(master, {offerIndex}), ter(temDISABLED));
            env.close();
            BEAST_EXPECT(ownerCount(env, master) == 0);
            BEAST_EXPECT(mintedCount(env, master) == 0);
            BEAST_EXPECT(burnedCount(env, master) == 0);

            env(token::acceptBuyOffer(master, offerIndex), ter(temDISABLED));
            env.close();
            BEAST_EXPECT(ownerCount(env, master) == 0);
            BEAST_EXPECT(mintedCount(env, master) == 0);
            BEAST_EXPECT(burnedCount(env, master) == 0);
        }
        {
            // If the NFT amendment is enabled all NFT-related
            // facilities should be available.
            Env env{*this, features};
            Account const& master = env.master;

            BEAST_EXPECT(ownerCount(env, master) == 0);
            BEAST_EXPECT(mintedCount(env, master) == 0);
            BEAST_EXPECT(burnedCount(env, master) == 0);

            uint256 const nftId0{token::getNextID(env, env.master, 0u)};
            env(token::mint(env.master, 0u));
            env.close();
            BEAST_EXPECT(ownerCount(env, master) == 1);
            BEAST_EXPECT(mintedCount(env, master) == 1);
            BEAST_EXPECT(burnedCount(env, master) == 0);

            env(token::burn(env.master, nftId0));
            env.close();
            BEAST_EXPECT(ownerCount(env, master) == 0);
            BEAST_EXPECT(mintedCount(env, master) == 1);
            BEAST_EXPECT(burnedCount(env, master) == 1);

            uint256 const nftId1{
                token::getNextID(env, env.master, 0u, tfTransferable)};
            env(token::mint(env.master, 0u), txflags(tfTransferable));
            env.close();
            BEAST_EXPECT(ownerCount(env, master) == 1);
            BEAST_EXPECT(mintedCount(env, master) == 2);
            BEAST_EXPECT(burnedCount(env, master) == 1);

            Account const alice{"alice"};
            env.fund(XRP(10000), alice);
            env.close();
            uint256 const aliceOfferIndex =
                keylet::nftoffer(alice, env.seq(alice)).key;
            env(token::createOffer(alice, nftId1, XRP(1000)),
                token::owner(master));
            env.close();

            BEAST_EXPECT(ownerCount(env, master) == 1);
            BEAST_EXPECT(mintedCount(env, master) == 2);
            BEAST_EXPECT(burnedCount(env, master) == 1);

            BEAST_EXPECT(ownerCount(env, alice) == 1);
            BEAST_EXPECT(mintedCount(env, alice) == 0);
            BEAST_EXPECT(burnedCount(env, alice) == 0);

            env(token::acceptBuyOffer(master, aliceOfferIndex));
            env.close();

            BEAST_EXPECT(ownerCount(env, master) == 0);
            BEAST_EXPECT(mintedCount(env, master) == 2);
            BEAST_EXPECT(burnedCount(env, master) == 1);

            BEAST_EXPECT(ownerCount(env, alice) == 1);
            BEAST_EXPECT(mintedCount(env, alice) == 0);
            BEAST_EXPECT(burnedCount(env, alice) == 0);
        }
    }

    void
    testMintReserve(FeatureBitset features)
    {
        // Verify that the reserve behaves as expected for minting.
        testcase("Mint reserve");

        using namespace test::jtx;

        Env env{*this, features};
        Account const alice{"alice"};
        Account const minter{"minter"};

        // Fund alice and minter enough to exist, but not enough to meet
        // the reserve for creating their first NFT.  Account reserve for unit
        // tests is 200 XRP, not 20.
        env.fund(XRP(200), alice, minter);
        env.close();
        BEAST_EXPECT(env.balance(alice) == XRP(200));
        BEAST_EXPECT(env.balance(minter) == XRP(200));
        BEAST_EXPECT(ownerCount(env, alice) == 0);
        BEAST_EXPECT(ownerCount(env, minter) == 0);

        // alice does not have enough XRP to cover the reserve for an NFT page.
        env(token::mint(alice, 0u), ter(tecINSUFFICIENT_RESERVE));
        env.close();
        BEAST_EXPECT(ownerCount(env, alice) == 0);
        BEAST_EXPECT(mintedCount(env, alice) == 0);
        BEAST_EXPECT(burnedCount(env, alice) == 0);

        // Pay alice almost enough to make the reserve for an NFT page.
        env(pay(env.master, alice, XRP(50) + drops(9)));
        env.close();

        // A lambda that checks alice's ownerCount, mintedCount, and
        // burnedCount all in one fell swoop.
        auto checkAliceOwnerMintedBurned = [&env, this, &alice](
                                               std::uint32_t owners,
                                               std::uint32_t minted,
                                               std::uint32_t burned,
                                               int line) {
            auto oneCheck =
                [line, this](
                    char const* type, std::uint32_t found, std::uint32_t exp) {
                    if (found == exp)
                        pass();
                    else
                    {
                        std::stringstream ss;
                        ss << "Wrong " << type << " count.  Found: " << found
                           << "; Expected: " << exp;
                        fail(ss.str(), __FILE__, line);
                    }
                };
            oneCheck("owner", ownerCount(env, alice), owners);
            oneCheck("minted", mintedCount(env, alice), minted);
            oneCheck("burned", burnedCount(env, alice), burned);
        };

        // alice still does not have enough XRP for the reserve of an NFT page.
        env(token::mint(alice, 0u), ter(tecINSUFFICIENT_RESERVE));
        env.close();
        checkAliceOwnerMintedBurned(0, 0, 0, __LINE__);

        // Pay alice enough to make the reserve for an NFT page.
        env(pay(env.master, alice, drops(11)));
        env.close();

        // Now alice can mint an NFT.
        env(token::mint(alice));
        env.close();
        checkAliceOwnerMintedBurned(1, 1, 0, __LINE__);

        // Alice should be able to mint an additional 31 NFTs without
        // any additional reserve requirements.
        for (int i = 1; i < 32; ++i)
        {
            env(token::mint(alice));
            checkAliceOwnerMintedBurned(1, i + 1, 0, __LINE__);
        }

        // That NFT page is full.  Creating an additional NFT page requires
        // additional reserve.
        env(token::mint(alice), ter(tecINSUFFICIENT_RESERVE));
        env.close();
        checkAliceOwnerMintedBurned(1, 32, 0, __LINE__);

        // Pay alice almost enough to make the reserve for an NFT page.
        env(pay(env.master, alice, XRP(50) + drops(329)));
        env.close();

        // alice still does not have enough XRP for the reserve of an NFT page.
        env(token::mint(alice), ter(tecINSUFFICIENT_RESERVE));
        env.close();
        checkAliceOwnerMintedBurned(1, 32, 0, __LINE__);

        // Pay alice enough to make the reserve for an NFT page.
        env(pay(env.master, alice, drops(11)));
        env.close();

        // Now alice can mint an NFT.
        env(token::mint(alice));
        env.close();
        checkAliceOwnerMintedBurned(2, 33, 0, __LINE__);

        // alice burns the NFTs she created: check that pages consolidate
        std::uint32_t seq = 0;

        while (seq < 33)
        {
            env(token::burn(alice, token::getID(alice, 0, seq++)));
            env.close();
            checkAliceOwnerMintedBurned((33 - seq) ? 1 : 0, 33, seq, __LINE__);
        }

        // alice burns a non-existent NFT.
        env(token::burn(alice, token::getID(alice, 197, 5)), ter(tecNO_ENTRY));
        env.close();
        checkAliceOwnerMintedBurned(0, 33, 33, __LINE__);

        // That was fun!  Now let's see what happens when we let someone else
        // mint NFTs on alice's behalf.  alice gives permission to minter.
        env(token::setMinter(alice, minter));
        env.close();
        BEAST_EXPECT(env.le(alice)->getAccountID(sfMinter) == minter.id());

        // A lambda that checks minter's and alice's ownerCount,
        // mintedCount, and burnedCount all in one fell swoop.
        auto checkMintersOwnerMintedBurned = [&env, this, &alice, &minter](
                                                 std::uint32_t aliceOwners,
                                                 std::uint32_t aliceMinted,
                                                 std::uint32_t aliceBurned,
                                                 std::uint32_t minterOwners,
                                                 std::uint32_t minterMinted,
                                                 std::uint32_t minterBurned,
                                                 int line) {
            auto oneCheck = [this](
                                char const* type,
                                std::uint32_t found,
                                std::uint32_t exp,
                                int line) {
                if (found == exp)
                    pass();
                else
                {
                    std::stringstream ss;
                    ss << "Wrong " << type << " count.  Found: " << found
                       << "; Expected: " << exp;
                    fail(ss.str(), __FILE__, line);
                }
            };
            oneCheck("alice owner", ownerCount(env, alice), aliceOwners, line);
            oneCheck(
                "alice minted", mintedCount(env, alice), aliceMinted, line);
            oneCheck(
                "alice burned", burnedCount(env, alice), aliceBurned, line);
            oneCheck(
                "minter owner", ownerCount(env, minter), minterOwners, line);
            oneCheck(
                "minter minted", mintedCount(env, minter), minterMinted, line);
            oneCheck(
                "minter burned", burnedCount(env, minter), minterBurned, line);
        };

        std::uint32_t nftSeq = 33;

        // Pay minter almost enough to make the reserve for an NFT page.
        env(pay(env.master, minter, XRP(50) - drops(1)));
        env.close();
        checkMintersOwnerMintedBurned(0, 33, nftSeq, 0, 0, 0, __LINE__);

        // minter still does not have enough XRP for the reserve of an NFT page.
        // Just for grins (and code coverage), minter mints NFTs that include
        // a URI.
        env(token::mint(minter),
            token::issuer(alice),
            token::uri("uri"),
            ter(tecINSUFFICIENT_RESERVE));
        env.close();
        checkMintersOwnerMintedBurned(0, 33, nftSeq, 0, 0, 0, __LINE__);

        // Pay minter enough to make the reserve for an NFT page.
        env(pay(env.master, minter, drops(11)));
        env.close();

        // Now minter can mint an NFT for alice.
        env(token::mint(minter), token::issuer(alice), token::uri("uri"));
        env.close();
        checkMintersOwnerMintedBurned(0, 34, nftSeq, 1, 0, 0, __LINE__);

        // Minter should be able to mint an additional 31 NFTs for alice
        // without any additional reserve requirements.
        for (int i = 1; i < 32; ++i)
        {
            env(token::mint(minter), token::issuer(alice), token::uri("uri"));
            checkMintersOwnerMintedBurned(0, i + 34, nftSeq, 1, 0, 0, __LINE__);
        }

        // Pay minter almost enough for the reserve of an additional NFT page.
        env(pay(env.master, minter, XRP(50) + drops(319)));
        env.close();

        // That NFT page is full.  Creating an additional NFT page requires
        // additional reserve.
        env(token::mint(minter),
            token::issuer(alice),
            token::uri("uri"),
            ter(tecINSUFFICIENT_RESERVE));
        env.close();
        checkMintersOwnerMintedBurned(0, 65, nftSeq, 1, 0, 0, __LINE__);

        // Pay minter enough for the reserve of an additional NFT page.
        env(pay(env.master, minter, drops(11)));
        env.close();

        // Now minter can mint an NFT.
        env(token::mint(minter), token::issuer(alice), token::uri("uri"));
        env.close();
        checkMintersOwnerMintedBurned(0, 66, nftSeq, 2, 0, 0, __LINE__);

        // minter burns the NFTs she created.
        while (nftSeq < 65)
        {
            env(token::burn(minter, token::getID(alice, 0, nftSeq++)));
            env.close();
            checkMintersOwnerMintedBurned(
                0, 66, nftSeq, (65 - seq) ? 1 : 0, 0, 0, __LINE__);
        }

        // minter has one more NFT to burn.  Should take her owner count to 0.
        env(token::burn(minter, token::getID(alice, 0, nftSeq++)));
        env.close();
        checkMintersOwnerMintedBurned(0, 66, nftSeq, 0, 0, 0, __LINE__);

        // minter burns a non-existent NFT.
        env(token::burn(minter, token::getID(alice, 2009, 3)),
            ter(tecNO_ENTRY));
        env.close();
        checkMintersOwnerMintedBurned(0, 66, nftSeq, 0, 0, 0, __LINE__);
    }

    void
    testMintInvalid(FeatureBitset features)
    {
        // Explore many of the invalid ways to mint an NFT.
        testcase("Mint invalid");

        using namespace test::jtx;

        Env env{*this, features};
        Account const alice{"alice"};
        Account const minter{"minter"};

        // Fund alice and minter enough to exist, but not enough to meet
        // the reserve for creating their first NFT.  Account reserve for unit
        // tests is 200 XRP, not 20.
        env.fund(XRP(200), alice, minter);
        env.close();

        env(token::mint(alice, 0u), ter(tecINSUFFICIENT_RESERVE));
        env.close();

        // Fund alice enough to start minting NFTs.
        env(pay(env.master, alice, XRP(1000)));
        env.close();

        //----------------------------------------------------------------------
        // preflight

        // Set a negative fee.
        env(token::mint(alice, 0u),
            fee(STAmount(10ull, true)),
            ter(temBAD_FEE));

        // Set an invalid flag.
        env(token::mint(alice, 0u), txflags(0x00008000), ter(temINVALID_FLAG));

        // Set a bad transfer fee.
        env(token::mint(alice, 0u),
            token::xferFee(maxTransferFee + 1),
            ter(temBAD_TRANSFER_FEE));

        // Account can't also be issuer.
        env(token::mint(alice, 0u), token::issuer(alice), ter(temMALFORMED));

        // Invalid URI: zero length.
        env(token::mint(alice, 0u), token::uri(""), ter(temMALFORMED));

        // Invalid URI: too long.
        env(token::mint(alice, 0u),
            token::uri(std::string(maxTokenURILength + 1, 'q')),
            ter(temMALFORMED));

        //----------------------------------------------------------------------
        // preflight

        // Non-existent issuer.
        env(token::mint(alice, 0u),
            token::issuer(Account("demon")),
            ter(tecNO_ISSUER));

        //----------------------------------------------------------------------
        // doApply

        // Existent issuer, but not given minting permission
        env(token::mint(minter, 0u),
            token::issuer(alice),
            ter(tecNO_PERMISSION));
    }

    void
    testBurnInvalid(FeatureBitset features)
    {
        // Explore many of the invalid ways to burn an NFT.
        testcase("Burn invalid");

        using namespace test::jtx;

        Env env{*this, features};
        Account const alice{"alice"};
        Account const buyer{"buyer"};
        Account const minter{"minter"};
        Account const gw("gw");
        IOU const gwAUD(gw["AUD"]);

        // Fund alice and minter enough to exist and create an NFT, but not
        // enough to meet the reserve for creating their first NFTOffer.
        // Account reserve for unit tests is 200 XRP, not 20.
        env.fund(XRP(250), alice, buyer, minter, gw);
        env.close();
        BEAST_EXPECT(ownerCount(env, alice) == 0);

        uint256 const nftAlice0ID =
            token::getNextID(env, alice, 0, tfTransferable);
        env(token::mint(alice, 0u), txflags(tfTransferable));
        env.close();
        BEAST_EXPECT(ownerCount(env, alice) == 1);

        //----------------------------------------------------------------------
        // preflight

        // Set a negative fee.
        env(token::burn(alice, nftAlice0ID),
            fee(STAmount(10ull, true)),
            ter(temBAD_FEE));
        env.close();
        BEAST_EXPECT(ownerCount(env, alice) == 1);

        // Set an invalid flag.
        env(token::burn(alice, nftAlice0ID),
            txflags(0x00008000),
            ter(temINVALID_FLAG));
        env.close();
        BEAST_EXPECT(ownerCount(env, buyer) == 0);

        //----------------------------------------------------------------------
        // preclaim

        // Try to burn a token that doesn't exist.
        env(token::burn(alice, token::getID(alice, 0, 1)), ter(tecNO_ENTRY));
        env.close();
        BEAST_EXPECT(ownerCount(env, buyer) == 0);

        // Can't burn a token with many buy or sell offers.  But that is
        // verified in testManyNftOffers().

        //----------------------------------------------------------------------
        // doApply
    }

    void
    testCreateOfferInvalid(FeatureBitset features)
    {
        testcase("Invalid NFT offer create");

        using namespace test::jtx;

        Env env{*this, features};
        Account const alice{"alice"};
        Account const buyer{"buyer"};
        Account const gw("gw");
        IOU const gwAUD(gw["AUD"]);

        // Fund alice enough to exist and create an NFT, but not
        // enough to meet the reserve for creating their first NFTOffer.
        // Account reserve for unit tests is 200 XRP, not 20.
        env.fund(XRP(250), alice, buyer, gw);
        env.close();
        BEAST_EXPECT(ownerCount(env, alice) == 0);

        uint256 const nftAlice0ID =
            token::getNextID(env, alice, 0, tfTransferable, 10);
        env(token::mint(alice, 0u),
            txflags(tfTransferable),
            token::xferFee(10));
        env.close();
        BEAST_EXPECT(ownerCount(env, alice) == 1);

        uint256 const nftXrpOnlyID =
            token::getNextID(env, alice, 0, tfOnlyXRP | tfTransferable);
        env(token::mint(alice, 0), txflags(tfOnlyXRP | tfTransferable));
        env.close();
        BEAST_EXPECT(ownerCount(env, alice) == 1);

        uint256 nftNoXferID = token::getNextID(env, alice, 0);
        env(token::mint(alice, 0));
        env.close();
        BEAST_EXPECT(ownerCount(env, alice) == 1);

        //----------------------------------------------------------------------
        // preflight

        // buyer burns a fee, so they no longer have enough XRP to cover the
        // reserve for a token offer.
        env(noop(buyer));
        env.close();

        // buyer tries to create an NFTokenOffer, but doesn't have the reserve.
        env(token::createOffer(buyer, nftAlice0ID, XRP(1000)),
            token::owner(alice),
            ter(tecINSUFFICIENT_RESERVE));
        env.close();
        BEAST_EXPECT(ownerCount(env, buyer) == 0);

        // Set a negative fee.
        env(token::createOffer(buyer, nftAlice0ID, XRP(1000)),
            fee(STAmount(10ull, true)),
            ter(temBAD_FEE));
        env.close();
        BEAST_EXPECT(ownerCount(env, buyer) == 0);

        // Set an invalid flag.
        env(token::createOffer(buyer, nftAlice0ID, XRP(1000)),
            txflags(0x00008000),
            ter(temINVALID_FLAG));
        env.close();
        BEAST_EXPECT(ownerCount(env, buyer) == 0);

        // Set an invalid amount.
        env(token::createOffer(buyer, nftXrpOnlyID, buyer["USD"](1)),
            ter(temBAD_AMOUNT));
        env(token::createOffer(buyer, nftAlice0ID, buyer["USD"](0)),
            ter(temBAD_AMOUNT));
        env(token::createOffer(buyer, nftXrpOnlyID, drops(0)),
            ter(temBAD_AMOUNT));
        env.close();
        BEAST_EXPECT(ownerCount(env, buyer) == 0);

        // Set a bad expiration.
        env(token::createOffer(buyer, nftAlice0ID, buyer["USD"](1)),
            token::expiration(0),
            ter(temBAD_EXPIRATION));
        env.close();
        BEAST_EXPECT(ownerCount(env, buyer) == 0);

        // Invalid Owner field and tfSellToken flag relationships.
        // A buy offer must specify the owner.
        env(token::createOffer(buyer, nftXrpOnlyID, XRP(1000)),
            ter(temMALFORMED));
        env.close();
        BEAST_EXPECT(ownerCount(env, buyer) == 0);

        // A sell offer must not specify the owner; the owner is implicit.
        env(token::createOffer(alice, nftXrpOnlyID, XRP(1000)),
            token::owner(alice),
            txflags(tfSellToken),
            ter(temMALFORMED));
        env.close();
        BEAST_EXPECT(ownerCount(env, alice) == 1);

        // An owner may not offer to buy their own token.
        env(token::createOffer(alice, nftXrpOnlyID, XRP(1000)),
            token::owner(alice),
            ter(temMALFORMED));
        env.close();
        BEAST_EXPECT(ownerCount(env, alice) == 1);

        // The destination may not be the account submitting the transaction.
        env(token::createOffer(alice, nftXrpOnlyID, XRP(1000)),
            token::destination(alice),
            txflags(tfSellToken),
            ter(temMALFORMED));
        env.close();
        BEAST_EXPECT(ownerCount(env, alice) == 1);

        // The destination must be an account already established in the ledger.
        env(token::createOffer(alice, nftXrpOnlyID, XRP(1000)),
            token::destination(Account("demon")),
            txflags(tfSellToken),
            ter(tecNO_DST));
        env.close();
        BEAST_EXPECT(ownerCount(env, alice) == 1);

        //----------------------------------------------------------------------
        // preclaim

        // The new NFTokenOffer may not have passed its expiration time.
        env(token::createOffer(buyer, nftXrpOnlyID, XRP(1000)),
            token::owner(alice),
            token::expiration(lastClose(env)),
            ter(tecEXPIRED));
        env.close();
        BEAST_EXPECT(ownerCount(env, buyer) == 0);

        // The nftID must be present in the ledger.
        env(token::createOffer(buyer, token::getID(alice, 0, 1), XRP(1000)),
            token::owner(alice),
            ter(tecNO_ENTRY));
        env.close();
        BEAST_EXPECT(ownerCount(env, buyer) == 0);

        // The nftID must be present in the ledger of a sell offer too.
        env(token::createOffer(alice, token::getID(alice, 0, 1), XRP(1000)),
            txflags(tfSellToken),
            ter(tecNO_ENTRY));
        env.close();
        BEAST_EXPECT(ownerCount(env, buyer) == 0);

        // buyer must have the funds to pay for their offer.
        env(token::createOffer(buyer, nftAlice0ID, gwAUD(1000)),
            token::owner(alice),
            ter(tecNO_LINE));
        env.close();
        BEAST_EXPECT(ownerCount(env, buyer) == 0);

        env(trust(buyer, gwAUD(1000)));
        env.close();
        BEAST_EXPECT(ownerCount(env, buyer) == 1);
        env.close();

        // Issuer (alice) must have a trust line for the offered funds.
        env(token::createOffer(buyer, nftAlice0ID, gwAUD(1000)),
            token::owner(alice),
            ter(tecNO_LINE));
        env.close();
        BEAST_EXPECT(ownerCount(env, buyer) == 1);

        // Give alice the needed trust line, but freeze it.
        env(trust(gw, alice["AUD"](999), tfSetFreeze));
        env.close();

        // Issuer (alice) must have a trust line for the offered funds and
        // the trust line may not be frozen.
        env(token::createOffer(buyer, nftAlice0ID, gwAUD(1000)),
            token::owner(alice),
            ter(tecFROZEN));
        env.close();
        BEAST_EXPECT(ownerCount(env, buyer) == 1);

        // Unfreeze alice's trustline.
        env(trust(gw, alice["AUD"](999), tfClearFreeze));
        env.close();

        // Can't transfer the NFT if the transferable flag is not set.
        env(token::createOffer(buyer, nftNoXferID, gwAUD(1000)),
            token::owner(alice),
            ter(tefTOKEN_IS_NOT_TRANSFERABLE));
        env.close();
        BEAST_EXPECT(ownerCount(env, buyer) == 1);

        // Give buyer the needed trust line, but freeze it.
        env(trust(gw, buyer["AUD"](999), tfSetFreeze));
        env.close();

        env(token::createOffer(buyer, nftAlice0ID, gwAUD(1000)),
            token::owner(alice),
            ter(tecFROZEN));
        env.close();
        BEAST_EXPECT(ownerCount(env, buyer) == 1);

        // Unfreeze buyer's trust line, but buyer has no actual gwAUD.
        // to cover the offer.
        env(trust(gw, buyer["AUD"](999), tfClearFreeze));
        env(trust(buyer, gwAUD(1000)));
        env.close();

        env(token::createOffer(buyer, nftAlice0ID, gwAUD(1000)),
            token::owner(alice),
            ter(tecUNFUNDED_OFFER));
        env.close();
        BEAST_EXPECT(ownerCount(env, buyer) == 1);  // the trust line.

        //----------------------------------------------------------------------
        // doApply

        // Give buyer almost enough AUD to cover the offer...
        env(pay(gw, buyer, gwAUD(999)));
        env.close();

        // However buyer doesn't have enough XRP to cover the reserve for
        // an NFT offer.
        env(token::createOffer(buyer, nftAlice0ID, gwAUD(1000)),
            token::owner(alice),
            ter(tecINSUFFICIENT_RESERVE));
        env.close();
        BEAST_EXPECT(ownerCount(env, buyer) == 1);

        // Give buyer almost enough XRP to cover the reserve.
        env(pay(env.master, buyer, XRP(50) + drops(119)));
        env.close();

        env(token::createOffer(buyer, nftAlice0ID, gwAUD(1000)),
            token::owner(alice),
            ter(tecINSUFFICIENT_RESERVE));
        env.close();
        BEAST_EXPECT(ownerCount(env, buyer) == 1);

        // Give buyer just enough XRP to cover the reserve for the offer.
        env(pay(env.master, buyer, drops(11)));
        env.close();

        // We don't care whether the offer is fully funded until the offer is
        // accepted.  Success at last!
        env(token::createOffer(buyer, nftAlice0ID, gwAUD(1000)),
            token::owner(alice),
            ter(tesSUCCESS));
        env.close();
        BEAST_EXPECT(ownerCount(env, buyer) == 2);
    }

    void
    testCancelOfferInvalid(FeatureBitset features)
    {
        testcase("Invalid NFT offer cancel");

        using namespace test::jtx;

        Env env{*this, features};
        Account const alice{"alice"};
        Account const buyer{"buyer"};
        Account const gw("gw");
        IOU const gwAUD(gw["AUD"]);

        env.fund(XRP(1000), alice, buyer, gw);
        env.close();
        BEAST_EXPECT(ownerCount(env, alice) == 0);

        uint256 const nftAlice0ID =
            token::getNextID(env, alice, 0, tfTransferable);
        env(token::mint(alice, 0u), txflags(tfTransferable));
        env.close();
        BEAST_EXPECT(ownerCount(env, alice) == 1);

        // This is the offer we'll try to cancel.
        uint256 const buyerOfferIndex =
            keylet::nftoffer(buyer, env.seq(buyer)).key;
        env(token::createOffer(buyer, nftAlice0ID, XRP(1)),
            token::owner(alice),
            ter(tesSUCCESS));
        env.close();
        BEAST_EXPECT(ownerCount(env, buyer) == 1);

        //----------------------------------------------------------------------
        // preflight

        // Set a negative fee.
        env(token::cancelOffer(buyer, {buyerOfferIndex}),
            fee(STAmount(10ull, true)),
            ter(temBAD_FEE));
        env.close();
        BEAST_EXPECT(ownerCount(env, buyer) == 1);

        // Set an invalid flag.
        env(token::cancelOffer(buyer, {buyerOfferIndex}),
            txflags(0x00008000),
            ter(temINVALID_FLAG));
        env.close();
        BEAST_EXPECT(ownerCount(env, buyer) == 1);

        // Empty list of tokens to delete.
        {
            Json::Value jv = token::cancelOffer(buyer);
            jv[sfTokenOffers.jsonName] = Json::arrayValue;
            env(jv, ter(temMALFORMED));
            env.close();
            BEAST_EXPECT(ownerCount(env, buyer) == 1);
        }

        // List of tokens to delete is too long.
        {
            std::vector<uint256> offers(
                maxTokenOfferCancelCount + 1, buyerOfferIndex);

            env(token::cancelOffer(buyer, offers), ter(temMALFORMED));
            env.close();
            BEAST_EXPECT(ownerCount(env, buyer) == 1);
        }

        // Duplicate entries are not allowed in the list of offers to cancel.
        env(token::cancelOffer(buyer, {buyerOfferIndex, buyerOfferIndex}),
            ter(temMALFORMED));
        env.close();
        BEAST_EXPECT(ownerCount(env, buyer) == 1);

        // Provide neither offers to cancel nor a root index.
        env(token::cancelOffer(buyer), ter(temMALFORMED));
        env.close();
        BEAST_EXPECT(ownerCount(env, buyer) == 1);

        //----------------------------------------------------------------------
        // preclaim

        // Make a non-root directory that we can pass as a root index.
        env(pay(env.master, gw, XRP(5000)));
        env.close();
        for (std::uint32_t i = 1; i < 34; ++i)
        {
            env(offer(gw, XRP(i), gwAUD(1)));
            env.close();
        }

        // gw attempts to cancel an offer they don't have permission to cancel.
        env(token::cancelOffer(gw, {buyerOfferIndex}), ter(tecNO_PERMISSION));
        env.close();
        BEAST_EXPECT(ownerCount(env, buyer) == 1);

        //----------------------------------------------------------------------
        // doApply
        //
        // The tefBAD_LEDGER conditions are too hard to test.
        // But let's see a successful offer cancel.
        env(token::cancelOffer(buyer, {buyerOfferIndex}));
        env.close();
        BEAST_EXPECT(ownerCount(env, buyer) == 0);
    }

    void
    testAcceptOfferInvalid(FeatureBitset features)
    {
        testcase("Invalid NFT offer accept");

        using namespace test::jtx;

        Env env{*this, features};
        Account const alice{"alice"};
        Account const buyer{"buyer"};
        Account const gw("gw");
        IOU const gwAUD(gw["AUD"]);

        env.fund(XRP(1000), alice, buyer, gw);
        env.close();
        BEAST_EXPECT(ownerCount(env, alice) == 0);

        uint256 const nftAlice0ID =
            token::getNextID(env, alice, 0, tfTransferable);
        env(token::mint(alice, 0u), txflags(tfTransferable));
        env.close();
        BEAST_EXPECT(ownerCount(env, alice) == 1);

        uint256 const nftXrpOnlyID =
            token::getNextID(env, alice, 0, tfOnlyXRP | tfTransferable);
        env(token::mint(alice, 0), txflags(tfOnlyXRP | tfTransferable));
        env.close();
        BEAST_EXPECT(ownerCount(env, alice) == 1);

        uint256 nftNoXferID = token::getNextID(env, alice, 0);
        env(token::mint(alice, 0));
        env.close();
        BEAST_EXPECT(ownerCount(env, alice) == 1);

        // alice creates sell offers for her nfts.
        uint256 const plainOfferIndex =
            keylet::nftoffer(alice, env.seq(alice)).key;
        env(token::createOffer(alice, nftAlice0ID, XRP(10)),
            txflags(tfSellToken));
        env.close();
        BEAST_EXPECT(ownerCount(env, alice) == 2);

        uint256 const xrpOnlyOfferIndex =
            keylet::nftoffer(alice, env.seq(alice)).key;
        env(token::createOffer(alice, nftXrpOnlyID, XRP(20)),
            txflags(tfSellToken));
        env.close();
        BEAST_EXPECT(ownerCount(env, alice) == 3);

        uint256 const noXferOfferIndex =
            keylet::nftoffer(alice, env.seq(alice)).key;
        env(token::createOffer(alice, nftNoXferID, XRP(30)),
            txflags(tfSellToken));
        env.close();
        BEAST_EXPECT(ownerCount(env, alice) == 4);

        // alice creates a sell offer that will expire soon.
        uint256 const aliceExpOfferIndex =
            keylet::nftoffer(alice, env.seq(alice)).key;
        env(token::createOffer(alice, nftNoXferID, XRP(40)),
            txflags(tfSellToken),
            token::expiration(lastClose(env) + 5));
        env.close();
        BEAST_EXPECT(ownerCount(env, alice) == 5);

        //----------------------------------------------------------------------
        // preflight

        // Set a negative fee.
        env(token::acceptSellOffer(buyer, noXferOfferIndex),
            fee(STAmount(10ull, true)),
            ter(temBAD_FEE));
        env.close();
        BEAST_EXPECT(ownerCount(env, buyer) == 0);

        // Set an invalid flag.
        env(token::acceptSellOffer(buyer, noXferOfferIndex),
            txflags(0x00008000),
            ter(temINVALID_FLAG));
        env.close();
        BEAST_EXPECT(ownerCount(env, buyer) == 0);

        // Supply nether an sfBuyOffer nor an sfSellOffer field.
        {
            Json::Value jv = token::acceptSellOffer(buyer, noXferOfferIndex);
            jv.removeMember(sfSellOffer.jsonName);
            env(jv, ter(temMALFORMED));
            env.close();
            BEAST_EXPECT(ownerCount(env, buyer) == 0);
        }

        // A buy offer may not contain an sfAmount field.
        {
            Json::Value jv = token::acceptBuyOffer(buyer, noXferOfferIndex);
            jv[sfAmount.jsonName] = STAmount(500000).getJson(JsonOptions::none);
            env(jv, ter(temMALFORMED));
            env.close();
            BEAST_EXPECT(ownerCount(env, buyer) == 0);
        }

        // A sell offer may not contain an sfAmount field.
        {
            Json::Value jv = token::acceptSellOffer(buyer, noXferOfferIndex);
            jv[sfAmount.jsonName] = STAmount(500000).getJson(JsonOptions::none);
            env(jv, ter(temMALFORMED));
            env.close();
            BEAST_EXPECT(ownerCount(env, buyer) == 0);
        }

        // A brokered offer must contain an sfAmount field.
        // Disable all tests of brokered offers for now.
        if (false)
        {
            Json::Value jv = token::brokerOffers(
                buyer, noXferOfferIndex, xrpOnlyOfferIndex, gwAUD(5));
            jv.removeMember(sfAmount.jsonName);
            env(jv, ter(temMALFORMED));
            env.close();
            BEAST_EXPECT(ownerCount(env, buyer) == 0);
        }

        //----------------------------------------------------------------------
        // preclaim

        // The buy offer must be present in the ledger.
        uint256 const missingOfferIndex = keylet::nftoffer(alice, 1).key;
        env(token::acceptBuyOffer(buyer, missingOfferIndex),
            ter(tecOBJECT_NOT_FOUND));
        env.close();
        BEAST_EXPECT(ownerCount(env, buyer) == 0);

        // the buy offer must not have expired.
        env(token::acceptBuyOffer(buyer, aliceExpOfferIndex), ter(tecEXPIRED));
        env.close();
        BEAST_EXPECT(ownerCount(env, buyer) == 0);

        // The sell offer must be present in the ledger.
        env(token::acceptSellOffer(buyer, missingOfferIndex),
            ter(tecOBJECT_NOT_FOUND));
        env.close();
        BEAST_EXPECT(ownerCount(env, buyer) == 0);

        // The sell offer must not have expired.
        env(token::acceptSellOffer(buyer, aliceExpOfferIndex), ter(tecEXPIRED));
        env.close();
        BEAST_EXPECT(ownerCount(env, buyer) == 0);

        //----------------------------------------------------------------------
        // preclaim brokered

        // alice and buyer need trustlines before buyer can to create an
        // offer for gwAUD.
        env(trust(alice, gwAUD(1000)));
        env(trust(buyer, gwAUD(1000)));
        env.close();
        env(pay(gw, buyer, gwAUD(30)));
        env.close();
        BEAST_EXPECT(ownerCount(env, alice) == 6);
        BEAST_EXPECT(ownerCount(env, buyer) == 1);

        // We're about to exercise offer brokering, so we need
        // corresponding buy and sell offers.
        //
        // buyer creates a buy offer for one of alice's nfts.
        uint256 const buyerOfferIndex =
            keylet::nftoffer(buyer, env.seq(buyer)).key;
        env(token::createOffer(buyer, nftAlice0ID, gwAUD(30)),
            token::owner(alice));
        env.close();
        BEAST_EXPECT(ownerCount(env, buyer) == 2);

        // gw attempts to broker offers that are not for the same token.
        // Disable all tests of brokered offers for now.
        if (false)
        {
            env(token::brokerOffers(
                    gw, buyerOfferIndex, xrpOnlyOfferIndex, XRP(5)),
                ter(tecBUY_SELL_MISMATCH));
            env.close();
            BEAST_EXPECT(ownerCount(env, buyer) == 2);

            // gw attempts to broker offers that are not for the same currency.
            env(token::brokerOffers(
                    gw, buyerOfferIndex, noXferOfferIndex, XRP(5)),
                ter(tecBUY_SELL_MISMATCH));
            env.close();
            BEAST_EXPECT(ownerCount(env, buyer) == 2);
        }

        //----------------------------------------------------------------------
        // preclaim buy

        // Don't accept a buy offer if the sell flag is set.
        env(token::acceptBuyOffer(buyer, plainOfferIndex),
            ter(tecOFFER_TYPE_MISMATCH));
        env.close();
        BEAST_EXPECT(ownerCount(env, alice) == 6);

        // An account can't accept its own offer.
        env(token::acceptBuyOffer(buyer, buyerOfferIndex),
            ter(tecCANT_ACCEPT_OWN_OFFER));
        env.close();
        BEAST_EXPECT(ownerCount(env, buyer) == 2);

        // An offer acceptor must have sufficient funds to pay for the offer.
        env(pay(buyer, gw, gwAUD(30)));
        env.close();
        BEAST_EXPECT(env.balance(buyer, gwAUD) == gwAUD(0));
        env(token::acceptBuyOffer(alice, buyerOfferIndex),
            ter(tecINSUFFICIENT_FUNDS));
        env.close();
        BEAST_EXPECT(ownerCount(env, buyer) == 2);

        //----------------------------------------------------------------------
        // preclaim sell

        // Don't accept a sell offer without the sell flag set.
        env(token::acceptSellOffer(alice, buyerOfferIndex),
            ter(tecOFFER_TYPE_MISMATCH));
        env.close();
        BEAST_EXPECT(ownerCount(env, alice) == 6);

        // An account can't accept its own offer.
        env(token::acceptSellOffer(alice, plainOfferIndex),
            ter(tecCANT_ACCEPT_OWN_OFFER));
        env.close();
        BEAST_EXPECT(ownerCount(env, buyer) == 2);

        // I've not found a way for the seller to _not_ own the token, so
        // that condition is untestable for now.

        // preflight forbids a sell offer with an Amount field.  So the
        // Amount field conditions are untestable.

        //----------------------------------------------------------------------
        // doApply brokered -- offer brokering is disabled for now.

        //----------------------------------------------------------------------
        // doApply accept buy

        // I'm not seeing any conditons that could lead to a buy accept failure
        // at this point other than a corrupt ledger.

        //----------------------------------------------------------------------
        // doApply accept sell

        // Insufficient funds.
        uint256 const gwAudOfferIndex =
            keylet::nftoffer(alice, env.seq(alice)).key;
        env(token::createOffer(alice, nftAlice0ID, gwAUD(50)),
            txflags(tfSellToken));
        env.close();
        BEAST_EXPECT(ownerCount(env, alice) == 7);

        BEAST_EXPECT(env.balance(buyer, gwAUD) == gwAUD(0));
        env(token::acceptSellOffer(buyer, gwAudOfferIndex),
            ter(tecINSUFFICIENT_FUNDS));
        env.close();
    }

    void
    testMintFlagBurnable(FeatureBitset features)
    {
        // Exercise NFTs with flagBurnable set and not set.
        testcase("Mint flagBurnable");

        using namespace test::jtx;

        Env env{*this, features};
        Account const alice{"alice"};
        Account const buyer{"buyer"};
        Account const minter1{"minter1"};
        Account const minter2{"minter2"};

        env.fund(XRP(1000), alice, buyer, minter1, minter2);
        env.close();
        BEAST_EXPECT(ownerCount(env, alice) == 0);

        // alice selects minter as her minter.
        env(token::setMinter(alice, minter1));
        env.close();

        // A lambda that...
        //  1. creates an alice nft
        //  2. minted by minter and
        //  3. transfers that nft to buyer.
        auto nftToBuyer = [&env, &alice, &minter1, &buyer](
                              std::uint32_t flags) {
            uint256 const nftID{token::getNextID(env, alice, 0u, flags)};
            env(token::mint(minter1, 0u), token::issuer(alice), txflags(flags));
            env.close();

            uint256 const offerIndex =
                keylet::nftoffer(minter1, env.seq(minter1)).key;
            env(token::createOffer(minter1, nftID, XRP(0)),
                txflags(tfSellToken));
            env.close();

            env(token::acceptSellOffer(buyer, offerIndex));
            env.close();

            return nftID;
        };

        // An NFT without flagBurnable can only be burned by its owner.
        {
            uint256 const noBurnID = nftToBuyer(0);
            env(token::burn(alice, noBurnID),
                token::owner(buyer),
                ter(tecNO_PERMISSION));
            env.close();
            env(token::burn(minter1, noBurnID),
                token::owner(buyer),
                ter(tecNO_PERMISSION));
            env.close();
            env(token::burn(minter2, noBurnID),
                token::owner(buyer),
                ter(tecNO_PERMISSION));
            env.close();

            BEAST_EXPECT(ownerCount(env, buyer) == 1);
            env(token::burn(buyer, noBurnID), token::owner(buyer));
            env.close();
            BEAST_EXPECT(ownerCount(env, buyer) == 0);
        }
        // An NFT with flagBurnable can be burned by the issuer.
        {
            uint256 const burnableID = nftToBuyer(tfBurnable);
            env(token::burn(minter2, burnableID),
                token::owner(buyer),
                ter(tecNO_PERMISSION));
            env.close();

            BEAST_EXPECT(ownerCount(env, buyer) == 1);
            env(token::burn(alice, burnableID), token::owner(buyer));
            env.close();
            BEAST_EXPECT(ownerCount(env, buyer) == 0);
        }
        // An NFT with flagBurnable can be burned by the owner.
        {
            uint256 const burnableID = nftToBuyer(tfBurnable);
            BEAST_EXPECT(ownerCount(env, buyer) == 1);
            env(token::burn(buyer, burnableID));
            env.close();
            BEAST_EXPECT(ownerCount(env, buyer) == 0);
        }
        // An NFT with flagBurnable can be burned by the minter.
        {
            uint256 const burnableID = nftToBuyer(tfBurnable);
            BEAST_EXPECT(ownerCount(env, buyer) == 1);
            env(token::burn(buyer, burnableID), token::owner(buyer));
            env.close();
            BEAST_EXPECT(ownerCount(env, buyer) == 0);
        }
        // An nft with flagBurnable may be burned by the issuers' minter,
        // who may not be the original minter.
        {
            uint256 const burnableID = nftToBuyer(tfBurnable);
            BEAST_EXPECT(ownerCount(env, buyer) == 1);

            env(token::setMinter(alice, minter2));
            env.close();

            // minter1 is no longer alice's minter, so no longer has
            // permisson to burn alice's nfts.
            env(token::burn(minter1, burnableID),
                token::owner(buyer),
                ter(tecNO_PERMISSION));
            env.close();
            BEAST_EXPECT(ownerCount(env, buyer) == 1);

            // minter2, however, can burn alice's nfts.
            env(token::burn(minter2, burnableID), token::owner(buyer));
            env.close();
            BEAST_EXPECT(ownerCount(env, buyer) == 0);
        }
    }

    void
    testMintFlagOnlyXRP(FeatureBitset features)
    {
        // Exercise NFTs with flagOnlyXRP set and not set.
        testcase("Mint flagOnlyXRP");

        using namespace test::jtx;

        Env env{*this, features};
        Account const alice{"alice"};
        Account const buyer{"buyer"};
        Account const gw("gw");
        IOU const gwAUD(gw["AUD"]);

        // Set trust lines so alice and buyer can use gwAUD.
        env.fund(XRP(1000), alice, buyer, gw);
        env.close();
        env(trust(alice, gwAUD(1000)));
        env(trust(buyer, gwAUD(1000)));
        env.close();
        env(pay(gw, buyer, gwAUD(100)));

        // Don't set flagOnlyXRP and offers can be made with IOUs.
        {
            uint256 const nftIOUsOkayID{
                token::getNextID(env, alice, 0u, tfTransferable)};
            env(token::mint(alice, 0u), txflags(tfTransferable));
            env.close();

            BEAST_EXPECT(ownerCount(env, alice) == 2);
            uint256 const aliceOfferIndex =
                keylet::nftoffer(alice, env.seq(alice)).key;
            env(token::createOffer(alice, nftIOUsOkayID, gwAUD(50)),
                txflags(tfSellToken));
            env.close();
            BEAST_EXPECT(ownerCount(env, alice) == 3);

            BEAST_EXPECT(ownerCount(env, buyer) == 1);
            uint256 const buyerOfferIndex =
                keylet::nftoffer(buyer, env.seq(buyer)).key;
            env(token::createOffer(buyer, nftIOUsOkayID, gwAUD(50)),
                token::owner(alice));
            env.close();
            BEAST_EXPECT(ownerCount(env, buyer) == 2);

            // Cancel the two offers just to be tidy.
            env(token::cancelOffer(alice, {aliceOfferIndex}));
            env(token::cancelOffer(buyer, {buyerOfferIndex}));
            env.close();
            BEAST_EXPECT(ownerCount(env, alice) == 2);
            BEAST_EXPECT(ownerCount(env, buyer) == 1);

            // Also burn alice's nft.
            env(token::burn(alice, nftIOUsOkayID));
            env.close();
            BEAST_EXPECT(ownerCount(env, alice) == 1);
        }

        // Set flagOnlyXRP and offers using IOUs are rejected.
        {
            uint256 const nftOnlyXRPID{
                token::getNextID(env, alice, 0u, tfOnlyXRP | tfTransferable)};
            env(token::mint(alice, 0u), txflags(tfOnlyXRP | tfTransferable));
            env.close();

            BEAST_EXPECT(ownerCount(env, alice) == 2);
            env(token::createOffer(alice, nftOnlyXRPID, gwAUD(50)),
                txflags(tfSellToken),
                ter(temBAD_AMOUNT));
            env.close();
            BEAST_EXPECT(ownerCount(env, alice) == 2);

            BEAST_EXPECT(ownerCount(env, buyer) == 1);
            env(token::createOffer(buyer, nftOnlyXRPID, gwAUD(50)),
                token::owner(alice),
                ter(temBAD_AMOUNT));
            env.close();
            BEAST_EXPECT(ownerCount(env, buyer) == 1);

            // However offers for XRP are okay.
            BEAST_EXPECT(ownerCount(env, alice) == 2);
            env(token::createOffer(alice, nftOnlyXRPID, XRP(60)),
                txflags(tfSellToken));
            env.close();
            BEAST_EXPECT(ownerCount(env, alice) == 3);

            BEAST_EXPECT(ownerCount(env, buyer) == 1);
            env(token::createOffer(buyer, nftOnlyXRPID, XRP(60)),
                token::owner(alice));
            env.close();
            BEAST_EXPECT(ownerCount(env, buyer) == 2);
        }
    }

    void
    testMintFlagCreateTrustLine(FeatureBitset features)
    {
        // Exercise NFTs with flagCreateTrustLines set and not set.
        testcase("Mint flagCreateTrustLines");

        using namespace test::jtx;

        Env env{*this, features};
        Account const alice{"alice"};
        Account const becky{"becky"};
        Account const cheri{"cheri"};
        Account const gw("gw");
        IOU const gwAUD(gw["AUD"]);
        IOU const gwCAD(gw["CAD"]);
        IOU const gwEUR(gw["EUR"]);

        env.fund(XRP(1000), alice, becky, cheri, gw);
        env.close();

        // Set trust lines so becky and cheri can use gw's currency.
        env(trust(becky, gwAUD(1000)));
        env(trust(cheri, gwAUD(1000)));
        env(trust(becky, gwCAD(1000)));
        env(trust(cheri, gwCAD(1000)));
        env(trust(becky, gwEUR(1000)));
        env(trust(cheri, gwEUR(1000)));
        env.close();
        env(pay(gw, becky, gwAUD(500)));
        env(pay(gw, becky, gwCAD(500)));
        env(pay(gw, becky, gwEUR(500)));
        env(pay(gw, cheri, gwAUD(500)));
        env(pay(gw, cheri, gwCAD(500)));
        env.close();

        // An nft without flagCreateTrustLines but with a non-zero transfer
        // fee will not allow creating offers that use IOUs for payment.
        for (std::uint32_t xferFee : {0, 1})
        {
            uint256 const nftNoAutoTrustID{
                token::getNextID(env, alice, 0u, tfTransferable, xferFee)};
            env(token::mint(alice, 0u),
                token::xferFee(xferFee),
                txflags(tfTransferable));
            env.close();

            // becky buys the nft for 1 drop.
            uint256 const beckyBuyOfferIndex =
                keylet::nftoffer(becky, env.seq(becky)).key;
            env(token::createOffer(becky, nftNoAutoTrustID, drops(1)),
                token::owner(alice));
            env.close();
            env(token::acceptBuyOffer(alice, beckyBuyOfferIndex));
            env.close();

            // becky attempts to sell the nft for AUD.
            TER const createOfferTER =
                xferFee ? TER(tecNO_LINE) : TER(tesSUCCESS);
            uint256 const beckyOfferIndex =
                keylet::nftoffer(becky, env.seq(becky)).key;
            env(token::createOffer(becky, nftNoAutoTrustID, gwAUD(100)),
                txflags(tfSellToken),
                ter(createOfferTER));
            env.close();

            // cheri offers to buy the nft for CAD.
            uint256 const cheriOfferIndex =
                keylet::nftoffer(cheri, env.seq(cheri)).key;
            env(token::createOffer(cheri, nftNoAutoTrustID, gwCAD(100)),
                token::owner(becky),
                ter(createOfferTER));
            env.close();

            // To keep things tidy, cancel the offers.
            env(token::cancelOffer(becky, {beckyOfferIndex}));
            env(token::cancelOffer(cheri, {cheriOfferIndex}));
            env.close();
        }
        // An nft with flagCreateTrustLines but with a non-zero transfer
        // fee allows transfers using IOUs for payment.
        {
            std::uint16_t transferFee = 10000;  // 10%

            uint256 const nftAutoTrustID{token::getNextID(
                env, alice, 0u, tfTransferable | tfTrustLine, transferFee)};
            env(token::mint(alice, 0u),
                token::xferFee(transferFee),
                txflags(tfTransferable | tfTrustLine));
            env.close();

            // becky buys the nft for 1 drop.
            uint256 const beckyBuyOfferIndex =
                keylet::nftoffer(becky, env.seq(becky)).key;
            env(token::createOffer(becky, nftAutoTrustID, drops(1)),
                token::owner(alice));
            env.close();
            env(token::acceptBuyOffer(alice, beckyBuyOfferIndex));
            env.close();

            // becky sells the nft for AUD.
            uint256 const beckySellOfferIndex =
                keylet::nftoffer(becky, env.seq(becky)).key;
            env(token::createOffer(becky, nftAutoTrustID, gwAUD(100)),
                txflags(tfSellToken));
            env.close();
            env(token::acceptSellOffer(cheri, beckySellOfferIndex));
            env.close();

            // alice should now have a trust line for gwAUD.
            BEAST_EXPECT(env.balance(alice, gwAUD) == gwAUD(10));

            // becky buys the nft back for CAD.
            uint256 const beckyBuyBackOfferIndex =
                keylet::nftoffer(becky, env.seq(becky)).key;
            env(token::createOffer(becky, nftAutoTrustID, gwCAD(50)),
                token::owner(cheri));
            env.close();
            env(token::acceptBuyOffer(cheri, beckyBuyBackOfferIndex));
            env.close();

            // alice should now have a trust line for gwAUD and gwCAD.
            BEAST_EXPECT(env.balance(alice, gwAUD) == gwAUD(10));
            BEAST_EXPECT(env.balance(alice, gwCAD) == gwCAD(5));
        }
        // Now that alice has trust lines already established, an nft without
        // flagCreateTrustLines will work for preestablished trust lines.
        {
            std::uint16_t transferFee = 5000;  // 5%
            uint256 const nftNoAutoTrustID{
                token::getNextID(env, alice, 0u, tfTransferable, transferFee)};
            env(token::mint(alice, 0u),
                token::xferFee(transferFee),
                txflags(tfTransferable));
            env.close();

            // alice sells the nft using AUD.
            uint256 const aliceSellOfferIndex =
                keylet::nftoffer(alice, env.seq(alice)).key;
            env(token::createOffer(alice, nftNoAutoTrustID, gwAUD(200)),
                txflags(tfSellToken));
            env.close();
            env(token::acceptSellOffer(cheri, aliceSellOfferIndex));
            env.close();

            // alice should now have AUD(210):
            //  o 200 for this sale and
            //  o 10 for the previous sale's fee.
            BEAST_EXPECT(env.balance(alice, gwAUD) == gwAUD(210));

            // cheri can't sell the NFT for EUR, but can for CAD.
            env(token::createOffer(cheri, nftNoAutoTrustID, gwEUR(50)),
                txflags(tfSellToken),
                ter(tecNO_LINE));
            env.close();
            uint256 const cheriSellOfferIndex =
                keylet::nftoffer(cheri, env.seq(cheri)).key;
            env(token::createOffer(cheri, nftNoAutoTrustID, gwCAD(100)),
                txflags(tfSellToken));
            env.close();
            env(token::acceptSellOffer(becky, cheriSellOfferIndex));
            env.close();

            // alice should now have CAD(10):
            //  o 5 from this sale's fee and
            //  o 5 for the previous sale's fee.
            BEAST_EXPECT(env.balance(alice, gwCAD) == gwCAD(10));
        }
    }

    void
    testMintFlagTransferable(FeatureBitset features)
    {
        // Exercise NFTs with flagTransferable set and not set.
        testcase("Mint flagTransferable");

        using namespace test::jtx;

        Env env{*this, features};

        Account const alice{"alice"};
        Account const becky{"becky"};
        Account const minter{"minter"};

        env.fund(XRP(1000), alice, becky, minter);
        env.close();

        // First try an nft made by alice without flagTransferable set.
        {
            BEAST_EXPECT(ownerCount(env, alice) == 0);
            uint256 const nftAliceNoTransferID{
                token::getNextID(env, alice, 0u)};
            env(token::mint(alice, 0u));
            env.close();
            BEAST_EXPECT(ownerCount(env, alice) == 1);

            // becky tries to offer to buy alice's nft.
            BEAST_EXPECT(ownerCount(env, becky) == 0);
            env(token::createOffer(becky, nftAliceNoTransferID, XRP(20)),
                token::owner(alice),
                ter(tefTOKEN_IS_NOT_TRANSFERABLE));

            // alice offers to sell the nft and becky accepts the offer.
            uint256 const aliceSellOfferIndex =
                keylet::nftoffer(alice, env.seq(alice)).key;
            env(token::createOffer(alice, nftAliceNoTransferID, XRP(20)),
                txflags(tfSellToken));
            env.close();
            env(token::acceptSellOffer(becky, aliceSellOfferIndex));
            env.close();
            BEAST_EXPECT(ownerCount(env, alice) == 0);
            BEAST_EXPECT(ownerCount(env, becky) == 1);

            // becky tries to offer the nft for sale.
            env(token::createOffer(becky, nftAliceNoTransferID, XRP(21)),
                txflags(tfSellToken),
                ter(tefTOKEN_IS_NOT_TRANSFERABLE));
            env.close();
            BEAST_EXPECT(ownerCount(env, alice) == 0);
            BEAST_EXPECT(ownerCount(env, becky) == 1);

            // becky tries to offer the nft for sale with alice as the
            // destination.  That also doesn't work.
            env(token::createOffer(becky, nftAliceNoTransferID, XRP(21)),
                txflags(tfSellToken),
                token::destination(alice),
                ter(tefTOKEN_IS_NOT_TRANSFERABLE));
            env.close();
            BEAST_EXPECT(ownerCount(env, alice) == 0);
            BEAST_EXPECT(ownerCount(env, becky) == 1);

            // alice offers to buy the nft back from becky.  becky accepts
            // the offer.
            uint256 const aliceBuyOfferIndex =
                keylet::nftoffer(alice, env.seq(alice)).key;
            env(token::createOffer(alice, nftAliceNoTransferID, XRP(22)),
                token::owner(becky));
            env.close();
            env(token::acceptBuyOffer(becky, aliceBuyOfferIndex));
            env.close();
            BEAST_EXPECT(ownerCount(env, alice) == 1);
            BEAST_EXPECT(ownerCount(env, becky) == 0);

            // alice burns her nft so accounting is simpler below.
            env(token::burn(alice, nftAliceNoTransferID));
            env.close();
            BEAST_EXPECT(ownerCount(env, alice) == 0);
            BEAST_EXPECT(ownerCount(env, becky) == 0);
        }
        // Try an nft minted by minter for alice without flagTransferable set.
        {
            env(token::setMinter(alice, minter));
            env.close();

            BEAST_EXPECT(ownerCount(env, minter) == 0);
            uint256 const nftMinterNoTransferID{
                token::getNextID(env, alice, 0u)};
            env(token::mint(minter), token::issuer(alice));
            env.close();
            BEAST_EXPECT(ownerCount(env, minter) == 1);

            // becky tries to offer to buy minter's nft.
            BEAST_EXPECT(ownerCount(env, becky) == 0);
            env(token::createOffer(becky, nftMinterNoTransferID, XRP(20)),
                token::owner(minter),
                ter(tefTOKEN_IS_NOT_TRANSFERABLE));
            env.close();
            BEAST_EXPECT(ownerCount(env, becky) == 0);

            // alice removes authorization of minter.
            env(token::clearMinter(alice));
            env.close();

            // minter tries to offer their nft for sale.
            BEAST_EXPECT(ownerCount(env, minter) == 1);
            env(token::createOffer(minter, nftMinterNoTransferID, XRP(21)),
                txflags(tfSellToken),
                ter(tefTOKEN_IS_NOT_TRANSFERABLE));
            env.close();
            BEAST_EXPECT(ownerCount(env, minter) == 1);

            // Let enough ledgers pass that old transactions are no longer
            // retried, then alice gives authorization back to minter.
            for (int i = 0; i < 10; ++i)
                env.close();

            env(token::setMinter(alice, minter));
            env.close();
            BEAST_EXPECT(ownerCount(env, minter) == 1);

            // minter successfully offers their nft for sale.
            BEAST_EXPECT(ownerCount(env, minter) == 1);
            uint256 const minterSellOfferIndex =
                keylet::nftoffer(minter, env.seq(minter)).key;
            env(token::createOffer(minter, nftMinterNoTransferID, XRP(22)),
                txflags(tfSellToken));
            env.close();
            BEAST_EXPECT(ownerCount(env, minter) == 2);

            // alice removes authorization of minter so we can see whether
            // minter's pre-existing offer still works.
            env(token::clearMinter(alice));
            env.close();

            // becky buys minter's nft even though minter is no longer alice's
            // official minter.
            BEAST_EXPECT(ownerCount(env, becky) == 0);
            env(token::acceptSellOffer(becky, minterSellOfferIndex));
            env.close();
            BEAST_EXPECT(ownerCount(env, becky) == 1);
            BEAST_EXPECT(ownerCount(env, minter) == 0);

            // becky attempts to sell the nft.
            env(token::createOffer(becky, nftMinterNoTransferID, XRP(23)),
                txflags(tfSellToken),
                ter(tefTOKEN_IS_NOT_TRANSFERABLE));
            env.close();

            // Since minter is not, at the moment, alice's official minter
            // they cannot create an offer to buy the nft they minted.
            BEAST_EXPECT(ownerCount(env, minter) == 0);
            env(token::createOffer(minter, nftMinterNoTransferID, XRP(24)),
                token::owner(becky),
                ter(tefTOKEN_IS_NOT_TRANSFERABLE));
            env.close();
            BEAST_EXPECT(ownerCount(env, minter) == 0);

            // alice can create an offer to buy the nft.
            BEAST_EXPECT(ownerCount(env, alice) == 0);
            uint256 const aliceBuyOfferIndex =
                keylet::nftoffer(alice, env.seq(alice)).key;
            env(token::createOffer(alice, nftMinterNoTransferID, XRP(25)),
                token::owner(becky));
            env.close();
            BEAST_EXPECT(ownerCount(env, alice) == 1);

            // Let enough ledgers pass that old transactions are no longer
            // retried, then alice gives authorization back to minter.
            for (int i = 0; i < 10; ++i)
                env.close();

            env(token::setMinter(alice, minter));
            env.close();

            // Now minter can create an offer to buy the nft.
            BEAST_EXPECT(ownerCount(env, minter) == 0);
            uint256 const minterBuyOfferIndex =
                keylet::nftoffer(minter, env.seq(minter)).key;
            env(token::createOffer(minter, nftMinterNoTransferID, XRP(26)),
                token::owner(becky));
            env.close();
            BEAST_EXPECT(ownerCount(env, minter) == 1);

            // alice removes authorization of minter so we can see whether
            // minter's pre-existing buy offer still works.
            env(token::clearMinter(alice));
            env.close();

            // becky accepts minter's sell offer.
            BEAST_EXPECT(ownerCount(env, minter) == 1);
            BEAST_EXPECT(ownerCount(env, becky) == 1);
            env(token::acceptBuyOffer(becky, minterBuyOfferIndex));
            env.close();
            BEAST_EXPECT(ownerCount(env, minter) == 1);
            BEAST_EXPECT(ownerCount(env, becky) == 0);
            BEAST_EXPECT(ownerCount(env, alice) == 1);

            // minter burns their nft and alice cancels her offer so the
            // next tests can start with a clean slate.
            env(token::burn(minter, nftMinterNoTransferID), ter(tesSUCCESS));
            env.close();
            env(token::cancelOffer(alice, {aliceBuyOfferIndex}));
            env.close();
            BEAST_EXPECT(ownerCount(env, alice) == 0);
            BEAST_EXPECT(ownerCount(env, becky) == 0);
            BEAST_EXPECT(ownerCount(env, minter) == 0);
        }
        // nfts with flagTransferable set should be buyable and salable
        // by anybody.
        {
            BEAST_EXPECT(ownerCount(env, alice) == 0);
            uint256 const nftAliceID{
                token::getNextID(env, alice, 0u, tfTransferable)};
            env(token::mint(alice, 0u), txflags(tfTransferable));
            env.close();
            BEAST_EXPECT(ownerCount(env, alice) == 1);

            // Both alice and becky can make offers for alice's nft.
            uint256 const aliceSellOfferIndex =
                keylet::nftoffer(alice, env.seq(alice)).key;
            env(token::createOffer(alice, nftAliceID, XRP(20)),
                txflags(tfSellToken));
            env.close();
            BEAST_EXPECT(ownerCount(env, alice) == 2);

            uint256 const beckyBuyOfferIndex =
                keylet::nftoffer(becky, env.seq(becky)).key;
            env(token::createOffer(becky, nftAliceID, XRP(21)),
                token::owner(alice));
            env.close();
            BEAST_EXPECT(ownerCount(env, alice) == 2);

            // becky accepts alice's sell offer.
            env(token::acceptSellOffer(becky, aliceSellOfferIndex));
            env.close();
            BEAST_EXPECT(ownerCount(env, alice) == 0);
            BEAST_EXPECT(ownerCount(env, becky) == 2);

            // becky offers to sell the nft.
            uint256 const beckySellOfferIndex =
                keylet::nftoffer(becky, env.seq(becky)).key;
            env(token::createOffer(becky, nftAliceID, XRP(22)),
                txflags(tfSellToken));
            env.close();
            BEAST_EXPECT(ownerCount(env, alice) == 0);
            BEAST_EXPECT(ownerCount(env, becky) == 3);

            // minter buys the nft (even though minter is not currently
            // alice's minter).
            env(token::acceptSellOffer(minter, beckySellOfferIndex));
            env.close();
            BEAST_EXPECT(ownerCount(env, alice) == 0);
            BEAST_EXPECT(ownerCount(env, becky) == 1);
            BEAST_EXPECT(ownerCount(env, minter) == 1);

            // minter offers to sell the nft.
            uint256 const minterSellOfferIndex =
                keylet::nftoffer(minter, env.seq(minter)).key;
            env(token::createOffer(minter, nftAliceID, XRP(23)),
                txflags(tfSellToken));
            env.close();
            BEAST_EXPECT(ownerCount(env, alice) == 0);
            BEAST_EXPECT(ownerCount(env, becky) == 1);
            BEAST_EXPECT(ownerCount(env, minter) == 2);

            // alice buys back the nft.
            env(token::acceptSellOffer(alice, minterSellOfferIndex));
            env.close();
            BEAST_EXPECT(ownerCount(env, alice) == 1);
            BEAST_EXPECT(ownerCount(env, becky) == 1);
            BEAST_EXPECT(ownerCount(env, minter) == 0);

            // Remember the buy offer that becky made for alice's token way
            // back when?  It's still in the ledger, and alice accepts it.
            env(token::acceptBuyOffer(alice, beckyBuyOfferIndex));
            env.close();
            BEAST_EXPECT(ownerCount(env, alice) == 0);
            BEAST_EXPECT(ownerCount(env, becky) == 1);
            BEAST_EXPECT(ownerCount(env, minter) == 0);

            // Just for tidyness, becky burns the token before shutting
            // things down.
            env(token::burn(becky, nftAliceID));
            env.close();
            BEAST_EXPECT(ownerCount(env, alice) == 0);
            BEAST_EXPECT(ownerCount(env, becky) == 0);
            BEAST_EXPECT(ownerCount(env, minter) == 0);
        }
    }

    void
    testMintTransferFee(FeatureBitset features)
    {
        // Exercise NFTs with and without a transferFee.
        testcase("Mint transferFee");

        using namespace test::jtx;

        Env env{*this, features};

        Account const alice{"alice"};
        Account const becky{"becky"};
        Account const carol{"carol"};
        Account const minter{"minter"};
        Account const gw{"gw"};
        IOU const gwXAU(gw["XAU"]);

        env.fund(XRP(1000), alice, becky, carol, minter, gw);
        env.close();

        env(trust(alice, gwXAU(2000)));
        env(trust(becky, gwXAU(2000)));
        env(trust(carol, gwXAU(2000)));
        env(trust(minter, gwXAU(2000)));
        env.close();
        env(pay(gw, alice, gwXAU(1000)));
        env(pay(gw, becky, gwXAU(1000)));
        env(pay(gw, carol, gwXAU(1000)));
        env(pay(gw, minter, gwXAU(1000)));
        env.close();

        // Giving alice a minter helps us see if transfer rates are affected
        // by that.
        env(token::setMinter(alice, minter));
        env.close();

        // If there is no transferFee, then alice gets nothing for the
        // transfer.
        {
            BEAST_EXPECT(ownerCount(env, alice) == 1);
            BEAST_EXPECT(ownerCount(env, becky) == 1);
            BEAST_EXPECT(ownerCount(env, carol) == 1);
            BEAST_EXPECT(ownerCount(env, minter) == 1);

            uint256 const nftID =
                token::getNextID(env, alice, 0u, tfTransferable);
            env(token::mint(alice), txflags(tfTransferable));
            env.close();

            // Becky buys the nft for XAU(10).  Check balances.
            uint256 const beckyBuyOfferIndex =
                keylet::nftoffer(becky, env.seq(becky)).key;
            env(token::createOffer(becky, nftID, gwXAU(10)),
                token::owner(alice));
            env.close();
            BEAST_EXPECT(env.balance(alice, gwXAU) == gwXAU(1000));
            BEAST_EXPECT(env.balance(becky, gwXAU) == gwXAU(1000));

            env(token::acceptBuyOffer(alice, beckyBuyOfferIndex));
            env.close();
            BEAST_EXPECT(env.balance(alice, gwXAU) == gwXAU(1010));
            BEAST_EXPECT(env.balance(becky, gwXAU) == gwXAU(990));

            // becky sells nft to carol.  alice's balance should not change.
            uint256 const beckySellOfferIndex =
                keylet::nftoffer(becky, env.seq(becky)).key;
            env(token::createOffer(becky, nftID, gwXAU(10)),
                txflags(tfSellToken));
            env.close();
            env(token::acceptSellOffer(carol, beckySellOfferIndex));
            env.close();
            BEAST_EXPECT(env.balance(alice, gwXAU) == gwXAU(1010));
            BEAST_EXPECT(env.balance(becky, gwXAU) == gwXAU(1000));
            BEAST_EXPECT(env.balance(carol, gwXAU) == gwXAU(990));

            // minter buys nft from carol.  alice's balance should not change.
            uint256 const minterBuyOfferIndex =
                keylet::nftoffer(minter, env.seq(minter)).key;
            env(token::createOffer(minter, nftID, gwXAU(10)),
                token::owner(carol));
            env.close();
            env(token::acceptBuyOffer(carol, minterBuyOfferIndex));
            env.close();
            BEAST_EXPECT(env.balance(alice, gwXAU) == gwXAU(1010));
            BEAST_EXPECT(env.balance(becky, gwXAU) == gwXAU(1000));
            BEAST_EXPECT(env.balance(carol, gwXAU) == gwXAU(1000));
            BEAST_EXPECT(env.balance(minter, gwXAU) == gwXAU(990));

            // minter sells the nft to alice.  gwXAU balances should finish
            // where they started.
            uint256 const minterSellOfferIndex =
                keylet::nftoffer(minter, env.seq(minter)).key;
            env(token::createOffer(minter, nftID, gwXAU(10)),
                txflags(tfSellToken));
            env.close();
            env(token::acceptSellOffer(alice, minterSellOfferIndex));
            env.close();
            BEAST_EXPECT(env.balance(alice, gwXAU) == gwXAU(1000));
            BEAST_EXPECT(env.balance(becky, gwXAU) == gwXAU(1000));
            BEAST_EXPECT(env.balance(carol, gwXAU) == gwXAU(1000));
            BEAST_EXPECT(env.balance(minter, gwXAU) == gwXAU(1000));

            // alice burns the nft to make later tests easier to think about.
            env(token::burn(alice, nftID));
            env.close();
            BEAST_EXPECT(ownerCount(env, alice) == 1);
            BEAST_EXPECT(ownerCount(env, becky) == 1);
            BEAST_EXPECT(ownerCount(env, carol) == 1);
            BEAST_EXPECT(ownerCount(env, minter) == 1);
        }

        // Set the smallest possible transfer fee.
        {
            // An nft with a transfer fee of 1 basis point.
            uint256 const nftID =
                token::getNextID(env, alice, 0u, tfTransferable, 1);
            env(token::mint(alice), txflags(tfTransferable), token::xferFee(1));
            env.close();

            // Becky buys the nft for XAU(10).  Check balances.
            uint256 const beckyBuyOfferIndex =
                keylet::nftoffer(becky, env.seq(becky)).key;
            env(token::createOffer(becky, nftID, gwXAU(10)),
                token::owner(alice));
            env.close();
            BEAST_EXPECT(env.balance(alice, gwXAU) == gwXAU(1000));
            BEAST_EXPECT(env.balance(becky, gwXAU) == gwXAU(1000));

            env(token::acceptBuyOffer(alice, beckyBuyOfferIndex));
            env.close();
            BEAST_EXPECT(env.balance(alice, gwXAU) == gwXAU(1010));
            BEAST_EXPECT(env.balance(becky, gwXAU) == gwXAU(990));

            // becky sells nft to carol.  alice's balance goes up.
            uint256 const beckySellOfferIndex =
                keylet::nftoffer(becky, env.seq(becky)).key;
            env(token::createOffer(becky, nftID, gwXAU(10)),
                txflags(tfSellToken));
            env.close();
            env(token::acceptSellOffer(carol, beckySellOfferIndex));
            env.close();

            BEAST_EXPECT(env.balance(alice, gwXAU) == gwXAU(1010.0001));
            BEAST_EXPECT(env.balance(becky, gwXAU) == gwXAU(999.9999));
            BEAST_EXPECT(env.balance(carol, gwXAU) == gwXAU(990));

            // minter buys nft from carol.  alice's balance goes up.
            uint256 const minterBuyOfferIndex =
                keylet::nftoffer(minter, env.seq(minter)).key;
            env(token::createOffer(minter, nftID, gwXAU(10)),
                token::owner(carol));
            env.close();
            env(token::acceptBuyOffer(carol, minterBuyOfferIndex));
            env.close();

            BEAST_EXPECT(env.balance(alice, gwXAU) == gwXAU(1010.0002));
            BEAST_EXPECT(env.balance(becky, gwXAU) == gwXAU(999.9999));
            BEAST_EXPECT(env.balance(carol, gwXAU) == gwXAU(999.9999));
            BEAST_EXPECT(env.balance(minter, gwXAU) == gwXAU(990));

            // minter sells the nft to alice.  Because alice is part of the
            // transaction no tranfer fee is removed.
            uint256 const minterSellOfferIndex =
                keylet::nftoffer(minter, env.seq(minter)).key;
            env(token::createOffer(minter, nftID, gwXAU(10)),
                txflags(tfSellToken));
            env.close();
            env(token::acceptSellOffer(alice, minterSellOfferIndex));
            env.close();
            BEAST_EXPECT(env.balance(alice, gwXAU) == gwXAU(1000.0002));
            BEAST_EXPECT(env.balance(becky, gwXAU) == gwXAU(999.9999));
            BEAST_EXPECT(env.balance(carol, gwXAU) == gwXAU(999.9999));
            BEAST_EXPECT(env.balance(minter, gwXAU) == gwXAU(1000));

            // alice pays to becky and carol so subsequent tests are easier
            // to think about.
            env(pay(alice, becky, gwXAU(0.0001)));
            env(pay(alice, carol, gwXAU(0.0001)));
            env.close();

            BEAST_EXPECT(env.balance(alice, gwXAU) == gwXAU(1000));
            BEAST_EXPECT(env.balance(becky, gwXAU) == gwXAU(1000));
            BEAST_EXPECT(env.balance(carol, gwXAU) == gwXAU(1000));
            BEAST_EXPECT(env.balance(minter, gwXAU) == gwXAU(1000));

            // alice burns the nft to make later tests easier to think about.
            env(token::burn(alice, nftID));
            env.close();
            BEAST_EXPECT(ownerCount(env, alice) == 1);
            BEAST_EXPECT(ownerCount(env, becky) == 1);
            BEAST_EXPECT(ownerCount(env, carol) == 1);
            BEAST_EXPECT(ownerCount(env, minter) == 1);
        }

        // Set the largest allowed transfer fee.
        {
            // A transfer fee greater than 50% is not allowed.
            env(token::mint(alice),
                txflags(tfTransferable),
                token::xferFee(maxTransferFee + 1),
                ter(temBAD_TRANSFER_FEE));
            env.close();

            // Make an nft with a transfer fee of 50%.
            uint256 const nftID = token::getNextID(
                env, alice, 0u, tfTransferable, maxTransferFee);
            env(token::mint(alice),
                txflags(tfTransferable),
                token::xferFee(maxTransferFee));
            env.close();

            // Becky buys the nft for XAU(10).  Check balances.
            uint256 const beckyBuyOfferIndex =
                keylet::nftoffer(becky, env.seq(becky)).key;
            env(token::createOffer(becky, nftID, gwXAU(10)),
                token::owner(alice));
            env.close();
            BEAST_EXPECT(env.balance(alice, gwXAU) == gwXAU(1000));
            BEAST_EXPECT(env.balance(becky, gwXAU) == gwXAU(1000));

            env(token::acceptBuyOffer(alice, beckyBuyOfferIndex));
            env.close();
            BEAST_EXPECT(env.balance(alice, gwXAU) == gwXAU(1010));
            BEAST_EXPECT(env.balance(becky, gwXAU) == gwXAU(990));

            // becky sells nft to minter.  alice's balance goes up.
            uint256 const beckySellOfferIndex =
                keylet::nftoffer(becky, env.seq(becky)).key;
            env(token::createOffer(becky, nftID, gwXAU(100)),
                txflags(tfSellToken));
            env.close();
            env(token::acceptSellOffer(minter, beckySellOfferIndex));
            env.close();

            BEAST_EXPECT(env.balance(alice, gwXAU) == gwXAU(1060));
            BEAST_EXPECT(env.balance(becky, gwXAU) == gwXAU(1040));
            BEAST_EXPECT(env.balance(minter, gwXAU) == gwXAU(900));

            // carol buys nft from minter.  alice's balance goes up.
            uint256 const carolBuyOfferIndex =
                keylet::nftoffer(carol, env.seq(carol)).key;
            env(token::createOffer(carol, nftID, gwXAU(10)),
                token::owner(minter));
            env.close();
            env(token::acceptBuyOffer(minter, carolBuyOfferIndex));
            env.close();

            BEAST_EXPECT(env.balance(alice, gwXAU) == gwXAU(1065));
            BEAST_EXPECT(env.balance(becky, gwXAU) == gwXAU(1040));
            BEAST_EXPECT(env.balance(minter, gwXAU) == gwXAU(905));
            BEAST_EXPECT(env.balance(carol, gwXAU) == gwXAU(990));

            // carol sells the nft to alice.  Because alice is part of the
            // transaction no tranfer fee is removed.
            uint256 const carolSellOfferIndex =
                keylet::nftoffer(carol, env.seq(carol)).key;
            env(token::createOffer(carol, nftID, gwXAU(10)),
                txflags(tfSellToken));
            env.close();
            env(token::acceptSellOffer(alice, carolSellOfferIndex));
            env.close();

            BEAST_EXPECT(env.balance(alice, gwXAU) == gwXAU(1055));
            BEAST_EXPECT(env.balance(becky, gwXAU) == gwXAU(1040));
            BEAST_EXPECT(env.balance(minter, gwXAU) == gwXAU(905));
            BEAST_EXPECT(env.balance(carol, gwXAU) == gwXAU(1000));

            // rebalance so subsequent tests are easier to think about.
            env(pay(alice, minter, gwXAU(55)));
            env(pay(becky, minter, gwXAU(40)));
            env.close();
            BEAST_EXPECT(env.balance(alice, gwXAU) == gwXAU(1000));
            BEAST_EXPECT(env.balance(becky, gwXAU) == gwXAU(1000));
            BEAST_EXPECT(env.balance(carol, gwXAU) == gwXAU(1000));
            BEAST_EXPECT(env.balance(minter, gwXAU) == gwXAU(1000));

            // alice burns the nft to make later tests easier to think about.
            env(token::burn(alice, nftID));
            env.close();
            BEAST_EXPECT(ownerCount(env, alice) == 1);
            BEAST_EXPECT(ownerCount(env, becky) == 1);
            BEAST_EXPECT(ownerCount(env, carol) == 1);
            BEAST_EXPECT(ownerCount(env, minter) == 1);
        }

        // See the impact of rounding when the nft is sold for small amounts
        // of drops.
        {
            // An nft with a transfer fee of 1 basis point.
            uint256 const nftID =
                token::getNextID(env, alice, 0u, tfTransferable, 1);
            env(token::mint(alice), txflags(tfTransferable), token::xferFee(1));
            env.close();

            // minter buys the nft for XRP(1).  Since the transfer involves
            // alice there should be no transfer fee.
            STAmount fee = drops(10);
            STAmount aliceBalance = env.balance(alice);
            STAmount minterBalance = env.balance(minter);
            uint256 const minterBuyOfferIndex =
                keylet::nftoffer(minter, env.seq(minter)).key;
            env(token::createOffer(minter, nftID, XRP(1)), token::owner(alice));
            env.close();
            env(token::acceptBuyOffer(alice, minterBuyOfferIndex));
            env.close();
            aliceBalance += XRP(1) - fee;
            minterBalance -= XRP(1) + fee;
            BEAST_EXPECT(env.balance(alice) == aliceBalance);
            BEAST_EXPECT(env.balance(minter) == minterBalance);

            // minter sells to carol.  The payment is just small enough that
            // alice does not get any transfer fee.
            STAmount carolBalance = env.balance(carol);
            uint256 const minterSellOfferIndex =
                keylet::nftoffer(minter, env.seq(minter)).key;
            env(token::createOffer(minter, nftID, drops(99999)),
                txflags(tfSellToken));
            env.close();
            env(token::acceptSellOffer(carol, minterSellOfferIndex));
            env.close();
            minterBalance += drops(99999) - fee;
            carolBalance -= drops(99999) + fee;
            BEAST_EXPECT(env.balance(alice) == aliceBalance);
            BEAST_EXPECT(env.balance(minter) == minterBalance);
            BEAST_EXPECT(env.balance(carol) == carolBalance);

            // carol sells to becky. This is the smallest amount to pay for a
            // transfer that enables a transfer fee of 1 basis point.
            STAmount beckyBalance = env.balance(becky);
            uint256 const beckyBuyOfferIndex =
                keylet::nftoffer(becky, env.seq(becky)).key;
            env(token::createOffer(becky, nftID, drops(100000)),
                token::owner(carol));
            env.close();
            env(token::acceptBuyOffer(carol, beckyBuyOfferIndex));
            env.close();
            carolBalance += drops(99999) - fee;
            beckyBalance -= drops(100000) + fee;
            aliceBalance += drops(1);

            BEAST_EXPECT(env.balance(alice) == aliceBalance);
            BEAST_EXPECT(env.balance(minter) == minterBalance);
            BEAST_EXPECT(env.balance(carol) == carolBalance);
            BEAST_EXPECT(env.balance(becky) == beckyBalance);
        }

        // See the impact of rounding when the nft is sold for small amounts
        // of an IOU.
        {
            // An nft with a transfer fee of 1 basis point.
            uint256 const nftID =
                token::getNextID(env, alice, 0u, tfTransferable, 1);
            env(token::mint(alice), txflags(tfTransferable), token::xferFee(1));
            env.close();

            // Due to the floating point nature of IOUs we need to
            // significantly reduce the gwXAU balances of our accounts prior
            // to the iou transfer.  Otherwise no transfers will happen.
            env(pay(alice, gw, env.balance(alice, gwXAU)));
            env(pay(minter, gw, env.balance(minter, gwXAU)));
            env(pay(becky, gw, env.balance(becky, gwXAU)));
            env.close();

            STAmount const startXAUBalance(
                gwXAU.issue(), STAmount::cMinValue, STAmount::cMinOffset + 5);
            env(pay(gw, alice, startXAUBalance));
            env(pay(gw, minter, startXAUBalance));
            env(pay(gw, becky, startXAUBalance));
            env.close();

            // Here is the smallest expressible gwXAU amount.
            STAmount tinyXAU(
                gwXAU.issue(), STAmount::cMinValue, STAmount::cMinOffset);

            // minter buys the nft for tinyXAU.  Since the transfer involves
            // alice there should be no transfer fee.
            STAmount aliceBalance = env.balance(alice, gwXAU);
            STAmount minterBalance = env.balance(minter, gwXAU);
            uint256 const minterBuyOfferIndex =
                keylet::nftoffer(minter, env.seq(minter)).key;
            env(token::createOffer(minter, nftID, tinyXAU),
                token::owner(alice));
            env.close();
            env(token::acceptBuyOffer(alice, minterBuyOfferIndex));
            env.close();
            aliceBalance += tinyXAU;
            minterBalance -= tinyXAU;
            BEAST_EXPECT(env.balance(alice, gwXAU) == aliceBalance);
            BEAST_EXPECT(env.balance(minter, gwXAU) == minterBalance);

            // minter sells to carol.
            STAmount carolBalance = env.balance(carol, gwXAU);
            uint256 const minterSellOfferIndex =
                keylet::nftoffer(minter, env.seq(minter)).key;
            env(token::createOffer(minter, nftID, tinyXAU),
                txflags(tfSellToken));
            env.close();
            env(token::acceptSellOffer(carol, minterSellOfferIndex));
            env.close();

            minterBalance += tinyXAU;
            carolBalance -= tinyXAU;
            // tiny XAU is so small that alice does not get a transfer fee.
            BEAST_EXPECT(env.balance(alice, gwXAU) == aliceBalance);
            BEAST_EXPECT(env.balance(minter, gwXAU) == minterBalance);
            BEAST_EXPECT(env.balance(carol, gwXAU) == carolBalance);

            // carol sells to becky.  This is the smallest gwXAU amount
            // to pay for a transfer that enables a transfer fee of 1.
            STAmount const cheapNFT(
                gwXAU.issue(), STAmount::cMinValue, STAmount::cMinOffset + 5);

            STAmount beckyBalance = env.balance(becky, gwXAU);
            uint256 const beckyBuyOfferIndex =
                keylet::nftoffer(becky, env.seq(becky)).key;
            env(token::createOffer(becky, nftID, cheapNFT),
                token::owner(carol));
            env.close();
            env(token::acceptBuyOffer(carol, beckyBuyOfferIndex));
            env.close();

            aliceBalance += tinyXAU;
            beckyBalance -= cheapNFT;
            carolBalance += cheapNFT - tinyXAU;
            BEAST_EXPECT(env.balance(alice, gwXAU) == aliceBalance);
            BEAST_EXPECT(env.balance(minter, gwXAU) == minterBalance);
            BEAST_EXPECT(env.balance(carol, gwXAU) == carolBalance);
            BEAST_EXPECT(env.balance(becky, gwXAU) == beckyBalance);
        }
    }

    void
    testMintTaxon(FeatureBitset features)
    {
        // Exercise the NFT taxon field.
        testcase("Mint taxon");

        using namespace test::jtx;

        Env env{*this, features};

        Account const alice{"alice"};
        Account const becky{"becky"};

        env.fund(XRP(1000), alice, becky);
        env.close();

        // The taxon field is incorporated straight into the NFT ID.  So
        // tests only need to operate on NFT IDs; we don't need to generate
        // any transactions.

        // The taxon value should be recoverable from the NFT ID.
        {
            uint256 const nftID = token::getNextID(env, alice, 0u);
            BEAST_EXPECT(nft::getTaxon(nftID) == 0);
        }

        // Do some touch testing to show that the taxon is recoverable no
        // matter what else changes around it in the nft ID.
        {
            std::uint32_t const taxon = {rand_int<std::uint32_t>()};
            for (int i = 0; i < 10; ++i)
            {
                // lambda to produce a useful message on error.
                auto check = [this](uint32_t taxon, uint256 const& nftID) {
                    std::uint32_t const gotTaxon = nft::getTaxon(nftID);
                    if (taxon == gotTaxon)
                        pass();
                    else
                    {
                        std::stringstream ss;
                        ss << "Taxon recovery failed from nftID "
                           << to_string(nftID) << ".  Expected: " << taxon
                           << "; got: " << gotTaxon;
                        fail(ss.str());
                    }
                };

                uint256 const nftAliceID = token::getID(
                    alice,
                    taxon,
                    rand_int<std::uint32_t>(),
                    rand_int<std::uint16_t>(),
                    rand_int<std::uint16_t>());
                check(taxon, nftAliceID);

                uint256 const nftBeckyID = token::getID(
                    becky,
                    taxon,
                    rand_int<std::uint32_t>(),
                    rand_int<std::uint16_t>(),
                    rand_int<std::uint16_t>());
                check(taxon, nftBeckyID);
            }
        }
    }

    void
    testMintURI(FeatureBitset features)
    {
        // Exercise the NFT URI field.
        //  1. Create a number of NFTs with and without URIs.
        //  2. Retrieve the NFTs from the server.
        //  3. Make sure the right URI is attached to each NFT.
        testcase("Mint URI");

        using namespace test::jtx;

        Env env{*this, features};

        Account const alice{"alice"};
        Account const becky{"becky"};

        env.fund(XRP(10000), alice, becky);
        env.close();

        // lambda that returns a randomly generated string which fits
        // the constraints of a URI.  Empty strings may be returned.
        // In the empty string case do not add the URI to the nft.
        auto randURI = []() {
            std::string ret;

            // About 20% of the returned strings should be empty
            if (rand_int(4) == 0)
                return ret;

            std::size_t const strLen = rand_int(256);
            ret.reserve(strLen);
            for (std::size_t i = 0; i < strLen; ++i)
                ret.push_back(rand_byte());

            return ret;
        };

        // Make a list of URIs that we'll put in nfts.
        struct Entry
        {
            std::string uri;
            std::uint32_t taxon;

            Entry(std::string uri_, std::uint32_t taxon_)
                : uri(std::move(uri_)), taxon(taxon_)
            {
            }
        };

        std::vector<Entry> entries;
        entries.reserve(100);
        for (std::size_t i = 0; i < 100; ++i)
            entries.emplace_back(randURI(), rand_int<std::uint32_t>());

        // alice creates nfts using entries.
        for (Entry const& entry : entries)
        {
            if (entry.uri.empty())
            {
                env(token::mint(alice, entry.taxon));
            }
            else
            {
                env(token::mint(alice, entry.taxon), token::uri(entry.uri));
            }
            env.close();
        }

        // Recover alice's nfts from the ledger.
        Json::Value aliceNFTs = [&env, &alice]() {
            Json::Value params;
            params[jss::account] = alice.human();
            params[jss::type] = "state";
            return env.rpc("json", "account_nfts", to_string(params));
        }();

        // Verify that the returned NFTs match what we sent.
        Json::Value& nfts = aliceNFTs[jss::result][jss::account_nfts];
        if (!BEAST_EXPECT(nfts.size() == entries.size()))
            return;

        // Sort the returned NFTs by nft_serial so the are in the same order
        // as entries.
        std::vector<Json::Value> sortedNFTs;
        sortedNFTs.reserve(nfts.size());
        for (std::size_t i = 0; i < nfts.size(); ++i)
            sortedNFTs.push_back(nfts[i]);
        std::sort(
            sortedNFTs.begin(),
            sortedNFTs.end(),
            [](Json::Value const& lhs, Json::Value const& rhs) {
                return lhs[jss::nft_serial] < rhs[jss::nft_serial];
            });

        for (std::size_t i = 0; i < entries.size(); ++i)
        {
            Entry const& entry = entries[i];
            Json::Value const& ret = sortedNFTs[i];
            BEAST_EXPECT(entry.taxon == ret[sfTokenTaxon.jsonName]);
            if (entry.uri.empty())
            {
                BEAST_EXPECT(!ret.isMember(sfURI.jsonName));
            }
            else
            {
                BEAST_EXPECT(strHex(entry.uri) == ret[sfURI.jsonName]);
            }
        }
    }

    void
    testBurn(FeatureBitset features)
    {
        // Exercise a number of conditions with NFT burning.
        testcase("Burn");

        using namespace test::jtx;

        Env env{*this, features};

        // Keep information associated with each account together.
        struct AcctStat
        {
            test::jtx::Account const acct;
            std::vector<uint256> nfts;

            AcctStat(char const* name) : acct(name)
            {
            }

            operator test::jtx::Account() const
            {
                return acct;
            }
        };
        AcctStat alice{"alice"};
        AcctStat becky{"becky"};
        AcctStat minter{"minter"};

        env.fund(XRP(10000), alice, becky, minter);
        env.close();

        // Both alice and minter mint nfts in case that makes any difference.
        env(token::setMinter(alice, minter));
        env.close();

        // Create enough NFTs that alice, becky, and minter can all have
        // at least three pages of NFTs.  This will cause more activity in
        // the page coalescing code.  If we make 210 NFTs in total, we can
        // have alice and minter each make 105.  That will allow us to
        // distribute 70 NFTs to our three participants.
        //
        // Give each NFT a pseudo-randomly chosen fee so the NFTs are
        // distributed pseudo-randomly through the pages.  This should
        // prevent alice's and minter's NFTs from clustering together
        // in becky's directory.
        //
        // Use a default initialized mercenne_twister because we want the
        // effect of random numbers, but we want the test to run the same
        // way each time.
        std::mt19937 engine;
        std::uniform_int_distribution<std::size_t> feeDist(
            decltype(maxTransferFee){}, maxTransferFee);

        alice.nfts.reserve(105);
        while (alice.nfts.size() < 105)
        {
            std::uint16_t const xferFee = feeDist(engine);
            alice.nfts.push_back(token::getNextID(
                env, alice, 0u, tfTransferable | tfBurnable, xferFee));
            env(token::mint(alice),
                txflags(tfTransferable | tfBurnable),
                token::xferFee(xferFee));
            env.close();
        }

        minter.nfts.reserve(105);
        while (minter.nfts.size() < 105)
        {
            std::uint16_t const xferFee = feeDist(engine);
            minter.nfts.push_back(token::getNextID(
                env, alice, 0u, tfTransferable | tfBurnable, xferFee));
            env(token::mint(minter),
                txflags(tfTransferable | tfBurnable),
                token::xferFee(xferFee),
                token::issuer(alice));
            env.close();
        }

        // All of the NFTs are now minted.  Transfer 35 each over to becky so
        // we end up with 70 NFTs in each account.
        becky.nfts.reserve(70);
        {
            auto aliceIter = alice.nfts.begin();
            auto minterIter = minter.nfts.begin();
            while (becky.nfts.size() < 70)
            {
                // We do the same work on alice and minter, so make a lambda.
                auto xferNFT = [&env, &becky](AcctStat& acct, auto& iter) {
                    uint256 offerIndex =
                        keylet::nftoffer(acct.acct, env.seq(acct.acct)).key;
                    env(token::createOffer(acct, *iter, XRP(0)),
                        txflags(tfSellToken));
                    env.close();
                    env(token::acceptSellOffer(becky, offerIndex));
                    env.close();
                    becky.nfts.push_back(*iter);
                    iter = acct.nfts.erase(iter);
                    iter += 2;
                };
                xferNFT(alice, aliceIter);
                xferNFT(minter, minterIter);
            }
            BEAST_EXPECT(aliceIter == alice.nfts.end());
            BEAST_EXPECT(minterIter == minter.nfts.end());
        }

        // A lambda that returns the number of nfts owned by an account.
        auto nftCount = [&env](AcctStat& acct) {
            Json::Value params;
            params[jss::account] = acct.acct.human();
            params[jss::type] = "state";
            Json::Value nfts =
                env.rpc("json", "account_nfts", to_string(params));
            return nfts[jss::result][jss::account_nfts].size();
        };

        // Now all three participants have 70 NFTs.
        BEAST_EXPECT(nftCount(alice) == 70);
        BEAST_EXPECT(nftCount(becky) == 70);
        BEAST_EXPECT(nftCount(minter) == 70);

        // Next we'll create offers for all of those NFTs.  This calls for
        // another lambda.
        auto addOffers =
            [&env](AcctStat& owner, AcctStat& other1, AcctStat& other2) {
                for (uint256 nft : owner.nfts)
                {
                    // Create sell offers for owner.
                    env(token::createOffer(owner, nft, drops(1)),
                        txflags(tfSellToken),
                        token::destination(other1));
                    env(token::createOffer(owner, nft, drops(1)),
                        txflags(tfSellToken),
                        token::destination(other2));
                    env.close();

                    // Create buy offers for other1 and other2.
                    env(token::createOffer(other1, nft, drops(1)),
                        token::owner(owner));
                    env(token::createOffer(other2, nft, drops(1)),
                        token::owner(owner));
                    env.close();

                    env(token::createOffer(other2, nft, drops(2)),
                        token::owner(owner));
                    env(token::createOffer(other1, nft, drops(2)),
                        token::owner(owner));
                    env.close();
                }
            };
        addOffers(alice, becky, minter);
        addOffers(becky, minter, alice);
        addOffers(minter, alice, becky);
        BEAST_EXPECT(ownerCount(env, alice) == 424);
        BEAST_EXPECT(ownerCount(env, becky) == 424);
        BEAST_EXPECT(ownerCount(env, minter) == 424);

        // Now each of the 270 NFTs has six offers associated with it.
        // Randomly select an NFT out of the pile and burn it.  Continue
        // the process until all NFTs are burned.
        AcctStat* const stats[3] = {&alice, &becky, &minter};
        std::uniform_int_distribution<std::size_t> acctDist(0, 2);
        std::uniform_int_distribution<std::size_t> mintDist(0, 1);

        while (stats[0]->nfts.size() > 0 || stats[1]->nfts.size() > 0 ||
               stats[2]->nfts.size() > 0)
        {
            // Pick an account to burn an nft.  If there are no nfts left
            // pick again.
            AcctStat& owner = *(stats[acctDist(engine)]);
            if (owner.nfts.empty())
                continue;

            // Pick one of the nfts.
            std::uniform_int_distribution<std::size_t> nftDist(
                0lu, owner.nfts.size() - 1);
            auto nftIter = owner.nfts.begin() + nftDist(engine);
            uint256 const nft = *nftIter;
            owner.nfts.erase(nftIter);

            // Decide which of the accounts should burn the nft.  If the
            // owner is becky then any of the three accounts can burn.
            // Otherwise either alice or minter can burn.
            AcctStat& burner = owner.acct == becky.acct
                ? *(stats[acctDist(engine)])
                : mintDist(engine) ? alice : minter;

            if (owner.acct == burner.acct)
                env(token::burn(burner, nft));
            else
                env(token::burn(burner, nft), token::owner(owner));
            env.close();

            // Every time we burn an nft, the number of nfts they hold should
            // match the number of nfts we think they hold.
            BEAST_EXPECT(nftCount(alice) == alice.nfts.size());
            BEAST_EXPECT(nftCount(becky) == becky.nfts.size());
            BEAST_EXPECT(nftCount(minter) == minter.nfts.size());
        }
        BEAST_EXPECT(nftCount(alice) == 0);
        BEAST_EXPECT(nftCount(becky) == 0);
        BEAST_EXPECT(nftCount(minter) == 0);

        // When all nfts are burned none of the accounts should have
        // an ownerCount.
        BEAST_EXPECT(ownerCount(env, alice) == 0);
        BEAST_EXPECT(ownerCount(env, becky) == 0);
        BEAST_EXPECT(ownerCount(env, minter) == 0);
    }

    void
    testBurnTooManyOffers(FeatureBitset features)
    {
        // Look at the case where too many offers prevents burning a token.
        testcase("Burn too many offers");

        using namespace test::jtx;

        Env env{*this, features};

        Account const alice("alice");
        Account const becky("becky");
        env.fund(XRP(1000), alice, becky);
        env.close();

        // We structure the test to try and maximize the metadata produced.
        // This verifies that we don't create too much metadata during a
        // maximal burn operation.
        //
        // 1. alice mints an nft with a full-sized URI.
        // 2. We create 1000 new accounts, each of which creates an offer for
        //    alice's nft.
        // 3. becky creates one more offer for alice's NFT
        // 4. Attempt to burn the nft which fails because there are too
        //    many offers.
        // 5. Cancel becky's offer and the nft should become burnable.
        uint256 const tokenID = token::getNextID(env, alice, 0, tfTransferable);
        env(token::mint(alice, 0),
            token::uri(std::string(maxTokenURILength, 'u')),
            txflags(tfTransferable));
        env.close();

        std::vector<uint256> offerIndexes;
        offerIndexes.reserve(maxTokenOfferCancelCount);
        for (uint32_t i = 0; i < maxTokenOfferCancelCount; ++i)
        {
            Account const acct(std::string("acct") + std::to_string(i));
            env.fund(XRP(1000), acct);
            env.close();

            offerIndexes.push_back(keylet::nftoffer(acct, env.seq(acct)).key);
            env(token::createOffer(acct, tokenID, drops(1)),
                token::owner(alice));
            env.close();
        }

        // Verify all offers are present in the ledger.
        for (uint256 const& offerIndex : offerIndexes)
        {
            BEAST_EXPECT(env.le(keylet::nftoffer(offerIndex)));
        }

        // Create one too many offers.
        uint256 const beckyOfferIndex =
            keylet::nftoffer(becky, env.seq(becky)).key;
        env(token::createOffer(becky, tokenID, drops(1)), token::owner(alice));

        // Attempt to burn the nft which should fail.
        env(token::burn(alice, tokenID), ter(tefTOO_BIG));

        // Close enough ledgers that the burn transaction is no longer retried.
        for (int i = 0; i < 10; ++i)
            env.close();

        // Cancel becky's offer, but alice adds a sell offer.  The token
        // should still not be burnable.
        env(token::cancelOffer(becky, {beckyOfferIndex}));
        env.close();

        uint256 const aliceOfferIndex =
            keylet::nftoffer(alice, env.seq(alice)).key;
        env(token::createOffer(alice, tokenID, drops(1)), txflags(tfSellToken));
        env.close();

        env(token::burn(alice, tokenID), ter(tefTOO_BIG));
        env.close();

        // Cancel alice's sell offer.  Now the token should be burnable.
        env(token::cancelOffer(alice, {aliceOfferIndex}));
        env.close();

        env(token::burn(alice, tokenID));
        env.close();

        // Burning the token should remove all the offers from the ledger.
        for (uint256 const& offerIndex : offerIndexes)
        {
            BEAST_EXPECT(!env.le(keylet::nftoffer(offerIndex)));
        }

        // Both alice and becky should have ownerCounts of zero.
        BEAST_EXPECT(ownerCount(env, alice) == 0);
        BEAST_EXPECT(ownerCount(env, becky) == 0);
    }

    void
    testCancelOffers(FeatureBitset features)
    {
        // Look at offer canceling.
        testcase("Cancel offers");

        using namespace test::jtx;

        Env env{*this, features};

        Account const alice("alice");
        Account const becky("becky");
        Account const minter("minter");
        env.fund(XRP(50000), alice, becky, minter);
        env.close();

        // alice has a minter to see if minters have offer canceling permission.
        env(token::setMinter(alice, minter));
        env.close();

        uint256 const tokenID = token::getNextID(env, alice, 0, tfTransferable);
        env(token::mint(alice, 0), txflags(tfTransferable));
        env.close();

        // Anyone can cancel an expired offer.
        uint256 const expiredOfferIndex =
            keylet::nftoffer(alice, env.seq(alice)).key;

        env(token::createOffer(alice, tokenID, XRP(1000)),
            txflags(tfSellToken),
            token::expiration(lastClose(env) + 13));
        env.close();

        // The offer has not expired yet, so becky can't cancel it now.
        BEAST_EXPECT(ownerCount(env, alice) == 2);
        env(token::cancelOffer(becky, {expiredOfferIndex}),
            ter(tecNO_PERMISSION));
        env.close();

        // Close a couple of ledgers and advance the time.  Then becky
        // should be able to cancel the (now) expired offer.
        env.close();
        env.close();
        env(token::cancelOffer(becky, {expiredOfferIndex}));
        env.close();
        BEAST_EXPECT(ownerCount(env, alice) == 1);

        // Create a couple of offers with a destination.  Those offers
        // should be cancellable by the creator and the destination.
        uint256 const dest1OfferIndex =
            keylet::nftoffer(alice, env.seq(alice)).key;

        env(token::createOffer(alice, tokenID, XRP(1000)),
            token::destination(becky),
            txflags(tfSellToken));
        env.close();
        BEAST_EXPECT(ownerCount(env, alice) == 2);

        // Minter can't cancel that offer, but becky (the destination) can.
        env(token::cancelOffer(minter, {dest1OfferIndex}),
            ter(tecNO_PERMISSION));
        env.close();
        BEAST_EXPECT(ownerCount(env, alice) == 2);

        env(token::cancelOffer(becky, {dest1OfferIndex}));
        env.close();
        BEAST_EXPECT(ownerCount(env, alice) == 1);

        // alice can cancel her own offer, even if becky is the destination.
        uint256 const dest2OfferIndex =
            keylet::nftoffer(alice, env.seq(alice)).key;

        env(token::createOffer(alice, tokenID, XRP(1000)),
            token::destination(becky),
            txflags(tfSellToken));
        env.close();
        BEAST_EXPECT(ownerCount(env, alice) == 2);

        env(token::cancelOffer(alice, {dest2OfferIndex}));
        env.close();
        BEAST_EXPECT(ownerCount(env, alice) == 1);

        // The issuer has no special permissions regarding offer cancellation.
        // Minter creates a token with alice as issuer.  alice cannot cancel
        // minter's offer.
        uint256 const mintersTokenID =
            token::getNextID(env, alice, 0, tfTransferable);
        env(token::mint(minter, 0),
            token::issuer(alice),
            txflags(tfTransferable));
        env.close();

        uint256 const minterOfferIndex =
            keylet::nftoffer(minter, env.seq(minter)).key;

        env(token::createOffer(minter, mintersTokenID, XRP(1000)),
            txflags(tfSellToken));
        env.close();
        BEAST_EXPECT(ownerCount(env, minter) == 2);

        // Nobody other than minter should be able to cancel minter's offer.
        env(token::cancelOffer(alice, {minterOfferIndex}),
            ter(tecNO_PERMISSION));
        env(token::cancelOffer(becky, {minterOfferIndex}),
            ter(tecNO_PERMISSION));
        env.close();
        BEAST_EXPECT(ownerCount(env, minter) == 2);

        env(token::cancelOffer(minter, {minterOfferIndex}));
        env.close();
        BEAST_EXPECT(ownerCount(env, minter) == 1);
    }

    void
    testCancelTooManyOffers(FeatureBitset features)
    {
        // Look at the case where too many offers are passed in a cancel.
        testcase("Cancel too many offers");

        using namespace test::jtx;

        Env env{*this, features};

        // We want to maximize the metadata from a cancel offer transaction to
        // make sure we don't hit metadata limits.  The way we'll do that is:
        //
        //  1. Generate twice as many separate funded accounts as we have
        //     offers.
        //  2.
        //     a. One of these accounts mints an NFT with a full URL.
        //     b. The other account makes an offer that will expire soon.
        //  3. After all of these offers have expired, cancel all of the
        //     expired offers in a single transaction.
        //
        // I can't think of any way to increase the metadata beyond this,
        // but I'm open to ideas.
        Account const alice("alice");
        env.fund(XRP(1000), alice);
        env.close();

        std::string const uri(maxTokenURILength, '?');
        std::vector<uint256> offerIndexes;
        offerIndexes.reserve(maxTokenOfferCancelCount + 1);
        for (uint32_t i = 0; i < maxTokenOfferCancelCount + 1; ++i)
        {
            Account const nftAcct(std::string("nftAcct") + std::to_string(i));
            Account const offerAcct(
                std::string("offerAcct") + std::to_string(i));
            env.fund(XRP(1000), nftAcct, offerAcct);
            env.close();

            uint256 const tokenID =
                token::getNextID(env, nftAcct, 0, tfTransferable);
            env(token::mint(nftAcct, 0),
                token::uri(uri),
                txflags(tfTransferable));
            env.close();

            offerIndexes.push_back(
                keylet::nftoffer(offerAcct, env.seq(offerAcct)).key);
            env(token::createOffer(offerAcct, tokenID, drops(1)),
                token::owner(nftAcct),
                token::expiration(lastClose(env) + 5));
            env.close();
        }

        // Close the ledger so the last of the offers expire.
        env.close();

        // All offers should be in the ledger.
        for (uint256 const& offerIndex : offerIndexes)
        {
            BEAST_EXPECT(env.le(keylet::nftoffer(offerIndex)));
        }

        // alice attempts to cancel all of the expired offers.  There is one
        // too many so the request fails.
        env(token::cancelOffer(alice, offerIndexes), ter(temMALFORMED));
        env.close();

        // However alice can cancel just one of the offers.
        env(token::cancelOffer(alice, {offerIndexes.back()}));
        env.close();

        // Verify that offer is gone from the ledger.
        BEAST_EXPECT(!env.le(keylet::nftoffer(offerIndexes.back())));
        offerIndexes.pop_back();

        // But alice adds a sell offer to the list...
        {
            uint256 const tokenID =
                token::getNextID(env, alice, 0, tfTransferable);
            env(token::mint(alice, 0),
                token::uri(uri),
                txflags(tfTransferable));
            env.close();

            offerIndexes.push_back(keylet::nftoffer(alice, env.seq(alice)).key);
            env(token::createOffer(alice, tokenID, drops(1)),
                txflags(tfSellToken));
            env.close();

            // alice's owner count should now to 2 for the nft and the offer.
            BEAST_EXPECT(ownerCount(env, alice) == 2);

            // Because alice added the sell offer there are still too many
            // offers in the list to cancel.
            env(token::cancelOffer(alice, offerIndexes), ter(temMALFORMED));
            env.close();

            // alice burns her nft which removes the nft and the offer.
            env(token::burn(alice, tokenID));
            env.close();

            // If alice's owner count is zero we can see that the offer
            // and nft are both gone.
            BEAST_EXPECT(ownerCount(env, alice) == 0);
            offerIndexes.pop_back();
        }

        // Now there are few enough offers in the list that they can all
        // be cancelled in a single transaction.
        env(token::cancelOffer(alice, offerIndexes));
        env.close();

        // Verify that remaining offers are gone from the ledger.
        for (uint256 const& offerIndex : offerIndexes)
        {
            BEAST_EXPECT(!env.le(keylet::nftoffer(offerIndex)));
        }
    }

    void
    testWithFeats(FeatureBitset features)
    {
        testEnabled(features);
        testMintReserve(features);
        testMintInvalid(features);
        testBurnInvalid(features);
        testCreateOfferInvalid(features);
        testCancelOfferInvalid(features);
        testAcceptOfferInvalid(features);
        testMintFlagBurnable(features);
        testMintFlagOnlyXRP(features);
        testMintFlagCreateTrustLine(features);
        testMintFlagTransferable(features);
        testMintTransferFee(features);
        testMintTaxon(features);
        testMintURI(features);
        testBurn(features);
        testBurnTooManyOffers(features);
        testCancelOffers(features);
        testCancelTooManyOffers(features);
    }

public:
    void
    run() override
    {
        using namespace test::jtx;
        auto const sa = supported_amendments();
        testWithFeats(sa);
    }
};

BEAST_DEFINE_TESTSUITE_PRIO(NFToken, tx, ripple, 3);

}  // namespace ripple
