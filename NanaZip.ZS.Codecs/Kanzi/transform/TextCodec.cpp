/*
Copyright 2011-2026 Frederic Langlet
Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
you may obtain a copy of the License at

                http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
*/

#include <cstring>
#include <stdexcept>
#include "TextCodec.hpp"
#include "../Global.hpp"
#include "../Magic.hpp"

using namespace kanzi;
using namespace std;


const int TextCodec::MAX_DICT_SIZE = 1 << 19; // must be less than 1<<24
const int TextCodec::MAX_WORD_LENGTH = 31; // must be less than 128
const int TextCodec::MIN_BLOCK_SIZE = 1024;
const int TextCodec::MAX_BLOCK_SIZE = 1 << 30; // 1 GB
const kanzi::byte TextCodec::ESCAPE_TOKEN1 = kanzi::byte(0x0F); // dictionary word preceded by space symbol
const kanzi::byte TextCodec::ESCAPE_TOKEN2 = kanzi::byte(0x0E); // toggle upper/lower case of first word char
const kanzi::byte TextCodec::MASK_1F = kanzi::byte(0x1F);
const kanzi::byte TextCodec::MASK_3F = kanzi::byte(0x3F);
const kanzi::byte TextCodec::MASK_20 = kanzi::byte(0x20);
const kanzi::byte TextCodec::MASK_40 = kanzi::byte(0x40);
const kanzi::byte TextCodec::MASK_80 = kanzi::byte(0x80);
const kanzi::byte TextCodec::MASK_FLIP_CASE = kanzi::byte(0x80);
const int TextCodec::HASH1 = 0x7FEB352D;
const int TextCodec::HASH2 = 0x846CA68B;
const kanzi::byte TextCodec::CR = kanzi::byte(0x0D);
const kanzi::byte TextCodec::LF = kanzi::byte(0x0A);
const kanzi::byte TextCodec::SP = kanzi::byte(0x20);
const int TextCodec::THRESHOLD1 = 128;
const int TextCodec::THRESHOLD2 = TextCodec::THRESHOLD1 * TextCodec::THRESHOLD1;
const int TextCodec::THRESHOLD3 = 64;
const int TextCodec::THRESHOLD4 = TextCodec::THRESHOLD3 * 128;
const int TextCodec::LOG_HASHES_SIZE = 24; // 16 MB
const kanzi::byte TextCodec::MASK_NOT_TEXT = kanzi::byte(0x80);
const kanzi::byte TextCodec::MASK_CRLF = kanzi::byte(0x40);
const kanzi::byte TextCodec::MASK_XML_HTML = kanzi::byte(0x20);
const kanzi::byte TextCodec::MASK_TEXT_CODEC = kanzi::byte(0x10);
const kanzi::byte TextCodec::MASK_DT = kanzi::byte(0x0F);
const int TextCodec::MASK_LENGTH = 0x0007FFFF; // 19-bit dictionary index

// V7 ranked dictionary indexes. The one-byte form has 63 usable values
// because 0x80 is reserved for the case-flip marker. The two-byte form has
// 8192 values, so the three-byte form starts at index 63 + 8192.
static const int V7_INDEX_BASE2 = 63;
static const int V7_INDEX_BASE3 = 8255;



// 1024 of the most common English words with at least 2 chars.
char TextCodec::DICT_EN_1024[] =
"TheBeAndOfInToWithItThatForYouHeHaveOnSaidSayAtButWeByHadTheyAsW\
ouldWhoOrCanMayDoThisWasIsMuchAnyFromNotSheWhatTheirWhichGetGive\
HasAreHimHerComeMyOurWereWillSomeBecauseThereThroughTellWhenWork\
ThemYetUpOwnOutIntoJustCouldOverOldThinkDayWayThanLikeOtherHowTh\
enItsPeopleTwoMoreTheseBeenNowWantFirstNewUseSeeTimeManManyThing\
MakeHereWellOnlyHisVeryAfterWithoutAnotherNoAllBelieveBeforeOffT\
houghSoAgainstWhileLastTooDownTodaySameBackTakeEachDifferentWher\
eBetweenThoseEvenSeenUnderAboutOneAlsoFactMustActuallyPreventExp\
ectContainConcernIfSchoolYearGoingCannotDueEverTowardGirlFirmGla\
ssGasKeepWorldStillWentShouldSpendStageDoctorMightJobGoContinueE\
veryoneNeverAnswerFewMeanDifferenceTendNeedLeaveTryNiceHoldSomet\
hingAskWarmLipCoverIssueHappenTurnLookSureDiscoverFightMadDirect\
ionAgreeSomeoneFailRespectNoticeChoiceBeginThreeSystemLevelFeelM\
eetCompanyBoxShowPlayLiveLetterEggNumberOpenProblemFatHandMeasur\
eQuestionCallRememberCertainPutNextChairStartRunRaiseGoalReallyH\
omeTeaCandidateMoneyBusinessYoungGoodCourtFindKnowKindHelpNightC\
hildLotYourUsEyeYesWordBitVanMonthHalfLowMillionHighOrganization\
RedGreenBlueWhiteBlackYourselfEightBothLittleHouseLetDespiteProv\
ideServiceHimselfFriendDescribeFatherDevelopmentAwayKillTripHour\
GameOftenPlantPlaceEndAmongSinceStandDesignParticularSuddenlyMem\
berPayLawBookSilenceAlmostIncludeAgainEitherToolFourOnceLeastExp\
lainIdentifyUntilSiteMinuteCoupleWeekMatterBringDetailInformatio\
nNothingAnythingEverythingAgoLeadSometimesUnderstandWhetherNatur\
eTogetherFollowParentStopIndeedDifficultPublicAlreadySpeakMainta\
inRemainHearAllowMediaOfficeBenefitDoorHugPersonLaterDuringWarHi\
storyArgueWithinSetArticleStationMorningWalkEventWinChooseBehavi\
orShootFireFoodTitleAroundAirTeacherGapSubjectEnoughProveAcrossA\
lthoughHeadFootSecondBoyMainLieAbleCivilTableLoveProcessOfferStu\
dentConsiderAppearStudyBuyNearlyHumanEvidenceTextMethodIncluding\
SendRealizeSenseBuildControlAudienceSeveralCutCollegeInterestSuc\
cessSpecialRiskExperienceBehindBetterResultTreatFiveRelationship\
AnimalImproveHairStayTopReducePerhapsLateWriterPickElseSignifica\
ntChanceHotelGeneralRockRequireAlongFitThemselvesReportCondition\
ReachTruthEffortDecideRateEducationForceGardenDrugLeaderVoiceQui\
teWholeSeemMindFinallySirReturnFreeStoryRespondPushAccordingBrot\
herLearnSonHopeDevelopFeelingReadCarryDiseaseRoadVariousBallCase\
OperationCloseVisitReceiveBuildingValueResearchFullModelJoinSeas\
onKnownDirectorPositionPlayerSportErrorRecordRowDataPaperTheoryS\
paceEveryFormSupportActionOfficialWhoseIdeaHappyHeartBestTeamPro\
jectHitBaseRepresentTownPullBusMapDryMomCatDadRoomSmileFieldImpa\
ctFundLargeDogHugePrepareEnvironmentalProduceHerselfTeachOilSuch\
SituationTieCostIndustrySkinStreetImageItselfPhonePriceWearMostS\
unSoonClearPracticePieceWaitRecentImportantProductLeftWallSeries\
NewsShareMovieKidNorSimplyWifeOntoCatchMyselfFineComputerSongAtt\
entionDrawFilmRepublicanSecurityScoreTestStockPositiveCauseCentu\
ryWindowMemoryExistListenStraightCultureBillionFormerDecisionEne\
rgyMoveSummerWonderRelateAvailableLineLikelyOutsideShotShortCoun\
tryRoleAreaSingleRuleDaughterMarketIndicatePresentLandCampaignMa\
terialPopulationEconomyMedicalHospitalChurchGroundThousandAuthor\
ityInsteadRecentlyFutureWrongInvolveLifeHeightIncreaseRightBankC\
ulturalCertainlyWestExecutiveBoardSeekLongOfficerStatementRestBa\
yDealWorkerResourceThrowForwardPolicyScienceEyesBedItemWeaponFil\
lPlanMilitaryGunHotHeatAddressColdFocusForeignTreatmentBloodUpon\
CourseThirdWatchAffectEarlyStoreThusSoundEverywhereBabyAdministr\
ationMouthPageEnterProbablyPointSeatNaturalRaceFarChallengePassA\
pplyMailUsuallyMixToughClearlyGrowFactorStateLocalGuyEastSaveSou\
thSceneMotherCareerQuicklyCentralFaceIceAboveBeyondPictureNetwor\
kManagementIndividualWomanSizeSpeedBusySeriousOccurAddReadySignC\
ollectionListApproachChargeQualityPressureVoteNotePartRealWebCur\
rentDetermineTrueSadWhateverBreakWorryCupParticularlyAmountAbili\
tyEatRecognizeSitCharacterSomebodyLossDegreeEffectAttackStaffMid\
dleTelevisionWhyLegalCapitalTradeElectionEverybodyDropMajorViewS\
tandardBillEmployeeDiscussionOpportunityAnalysisTenSuggestLawyer\
HusbandSectionBecomeSkillSisterStyleCrimeProgramCompareCapMissBa\
dSortTrainingEasyNearRegionStrategyPurposePerformTechnologyEcono\
micBudgetExampleCheckEnvironmentDoneDarkTermRatherLaughGuessCarL\
owerHangPastSocialForgetHundredRemoveManagerEnjoyExactlyDieFinal\
MaybeHealthFloorChangeAmericanPoorFunEstablishTrialSpringDinnerB\
igThankProtectAvoidImagineTonightStarArmFinishMusicOwnerCryArtPr\
ivateOthersSimplePopularReflectEspeciallySmallLightMessageStepKe\
yPeaceProgressMadeSideGreatFixInterviewManageNationalFishLoseCam\
eraDiscussEqualWeightPerformanceSevenWaterProductionPersonalCell\
PowerEveningColorInsideBarUnitLessAdultWideRangeMentionDeepEdgeS\
trongHardTroubleNecessarySafeCommonFearFamilySeaDreamConferenceR\
eplyPropertyMeetingAlwaysStuffAgencyDeathGrowthSellSoldierActHea\
vyWetBagMarriageDeadSingRiseDecadeWhomFigurePoliceBodyMachineCat\
egoryAheadFrontCareOrderRealityPartnerYardBeatViolenceTotalDefen\
seWriteConsumerCenterGroupThoughtModernTaskCoachReasonAgeFingerS\
pecificConnectionWishResponsePrettyMovementCardLogNumberSumTreeE\
ntireCitizenThroughoutPetSimilarVictimNewspaperThreatClassShakeS\
ourceAccountPainFallRichPossibleAcceptSolidTravelTalkSaidCreateN\
onePlentyPeriodDefineNormalRevealDrinkAuthorServeNameMomentAgent\
DocumentActivityAnywayAfraidTypeActiveTrainInterestingRadioDange\
rGenerationLeafCopyMatchClaimAnyoneSoftwarePartyDeviceCodeLangua\
geLinkHoweverConfirmCommentCityAnywhereSomewhereDebateDriveHighe\
rBeautifulOnlineFanPriorityTraditionalSixUnited";


DictEntry TextCodec::STATIC_DICTIONARY[1024] = {};
int8 TextCodec::CHAR_TYPE[256] = {};
const bool TextCodec::INIT = TextCodec::init(TextCodec::CHAR_TYPE);
const int TextCodec::STATIC_DICT_WORDS = TextCodec::createDictionary(DICT_EN_1024, sizeof(DICT_EN_1024), STATIC_DICTIONARY, 1024, 0);

bool TextCodec::init(int8 cType[256])
{
    for (int i = 0; i < 256; i++) {
        if ((i >= ' ') && (i <= '/')) // [ !"#$%&'()*+,-./]
            cType[i] = 1;
        else if ((i >= ':') && (i <= '?')) // [:;<=>?]
            cType[i] = 1;
        else {
            switch (i) {
            case '\n':
            case '\r':
            case '\t':
            case '_':
            case '|':
            case '{':
            case '}':
            case '[':
            case ']':
                cType[i] = 1;
                break;
            default:
                cType[i] = (isUpperCase(byte(i)) || isLowerCase(byte(i))) == true ? 0 : -1;
            }
        }
    }

    return true;
}

// Create dictionary from array of words
int TextCodec::createDictionary(char words[], int dictSize, DictEntry dict[], int maxWords, int startWord)
{
    int delimAnchor = 0;
    int nbWords = startWord;
    byte* src = reinterpret_cast<byte*>(words);

    for (int i = 0; ((i < dictSize) && (nbWords < maxWords)); i++) {
        if (isText(src[i]) == false)
            continue;

        if (isUpperCase(src[i])) {
            if (i > delimAnchor) {
                const int length = i - delimAnchor;
                const uint h = computeWordHash(&src[delimAnchor], length);
                dict[nbWords] = DictEntry(&src[delimAnchor], h, nbWords, length);
                nbWords++;
                delimAnchor = i;
            }

            src[i] ^= byte(0x20);
        }
    }

    if (nbWords < maxWords) {
        const int length = dictSize - 1 - delimAnchor;
        const uint h = computeWordHash(&src[delimAnchor], length);
        dict[nbWords] = DictEntry(&src[delimAnchor], h, nbWords, length);
        nbWords++;
    }

    return nbWords;
}

// Analyze the block and return an 8-bit status (see MASK flags constants)
// The goal is to detect text data amenable to pre-processing.
byte TextCodec::computeStats(const byte block[], int count, uint freqs0[], bool strict)
{
    if ((strict == false) && (Magic::getType(block) != Magic::NO_MAGIC)) {
        // This is going to fail if the block is not the first of the file.
        // But this is a cheap test, good enough for fast mode.
        return TextCodec::MASK_NOT_TEXT;
    }

    uint* freqs1 = new uint[65536];
    memset(&freqs1[0], 0, 65536 * sizeof(uint));
    uint f0[256] = { 0 };
    uint f1[256] = { 0 };
    uint f3[256] = { 0 };
    uint f2[256] = { 0 };
    uint8 prv = 0;
    const uint8* data = reinterpret_cast<const uint8*>(&block[0]);
    const int count4 = count & -4;

    // Unroll loop
    for (int i = 0; i < count4; i += 4) {
        const uint8 cur0 = data[i];
        const uint8 cur1 = data[i + 1];
        const uint8 cur2 = data[i + 2];
        const uint8 cur3 = data[i + 3];
        f0[cur0]++;
        f1[cur1]++;
        f2[cur2]++;
        f3[cur3]++;
        freqs1[(prv  * 256) + cur0]++;
        freqs1[(cur0 * 256) + cur1]++;
        freqs1[(cur1 * 256) + cur2]++;
        freqs1[(cur2 * 256) + cur3]++;
        prv = cur3;
    }

    for (int i = count4; i < count; i++) {
        freqs0[data[i]]++;
        freqs1[(prv * 256) + data[i]]++;
        prv = data[i];
    }

    for (int i = 0; i < 256; i++) {
        freqs0[i] += (f0[i] + f1[i] + f2[i] + f3[i]);
    }

    // Reject simple alphabets before the text heuristic. Ignore line
    // whitespace so wrapped DNA, Base64, and numeric data are detected too.
    uint freqsSimple[256];
    memcpy(&freqsSimple[0], &freqs0[0], sizeof(freqsSimple));
    const int nbWhitespace = freqsSimple[' '] + freqsSimple['\t'] +
                             freqsSimple['\n'] + freqsSimple['\r'];
    const int simpleCount = count - nbWhitespace;
    freqsSimple[' '] = 0;
    freqsSimple['\t'] = 0;
    freqsSimple['\n'] = 0;
    freqsSimple['\r'] = 0;
    const Global::DataType simpleType = Global::detectSimpleType(simpleCount, freqsSimple);

    if (simpleType != Global::UNDEFINED) {
        delete[] freqs1;
        return TextCodec::MASK_NOT_TEXT | byte(simpleType);
    }

    const int cr = int(CR);
    const int lf = int(LF);
    int nbTextChars = freqs0[cr] + freqs0[lf];
    int nbASCII = 0;

    for (int i = 0; i < 128; i++) {
        if (isText(byte(i)) == true)
            nbTextChars += freqs0[i];

        nbASCII += freqs0[i];
    }

    // Not text (crude thresholds)
    const int nbBinChars = count - nbASCII;
    bool notText = nbBinChars > (count >> 2);

    if (notText == false) {
        notText = nbTextChars < (count >> 2);

        if (strict == true) {
            notText |= ((freqs0[0] >= uint(count / 100)) || ((nbASCII / 95) < (count / 100)));
        } else {
            notText |= (freqs0[32] < uint(count / 50));
        }
    }

    byte res = byte(0);

    if (notText == true) {
        res = detectType(freqs0, freqs1, count);
        delete[] freqs1;
        return res;
    }

    if (nbBinChars <= count - count / 10) {
        // Check if likely XML/HTML
        // Another crude test: check that the frequencies of < and > are similar
        // and 'high enough'. Also check it is worth to attempt replacing ampersand sequences.
        // Getting this flag wrong results in a very small compression speed degradation.
        const int f60 = freqs0[60]; // '<'
        const int f62 = freqs0[62]; // '>'
        const int f38 = freqs1[38 * 256 + 97]  + freqs1[38 * 256 + 103] +
                        freqs1[38 * 256 + 108] + freqs1[38 * 256 + 113]; // '&a', '&g', '&l', '&q'
        const int minFreq = max((count - nbBinChars) >> 9, 2);

        if ((f60 >= minFreq) && (f62 >= minFreq) && (f38 > 0)) {
            if (f60 < f62) {
                if (f60 >= (f62 - f62 / 100))
                    res |= TextCodec::MASK_XML_HTML;
            }
            else if (f62 < f60) {
                if (f62 >= (f60 - f60 / 100))
                    res |= TextCodec::MASK_XML_HTML;
            }
            else {
                res |= TextCodec::MASK_XML_HTML;
            }
        }
    }

    // Check CR+LF matches
    if ((freqs0[cr] != 0) && (freqs0[cr] == freqs0[lf])) {
        res |= TextCodec::MASK_CRLF;

        for (int i = 0; i < 256; i++) {
            if ((i != lf) && (freqs1[(cr * 256) + i]) != 0) {
                res &= ~TextCodec::MASK_CRLF;
                break;
            }

            if ((i != cr) && (freqs1[(i * 256) + lf]) != 0) {
                res &= ~TextCodec::MASK_CRLF;
                break;
            }
        }
    }

    delete[] freqs1;
    return res;
}

byte TextCodec::detectType(const uint freqs0[], const uint freqs1[], int count) {
    Global::DataType dt = Global::detectSimpleType(count, freqs0);

    if (dt != Global::UNDEFINED)
       return TextCodec::MASK_NOT_TEXT | byte(dt);

    // Valid UTF-8 sequences
    // See Unicode 16 Standard - UTF-8 Table 3.7
    // U+0000..U+007F          00..7F
    // U+0080..U+07FF          C2..DF 80..BF
    // U+0800..U+0FFF          E0 A0..BF 80..BF
    // U+1000..U+CFFF          E1..EC 80..BF 80..BF
    // U+D000..U+D7FF          ED 80..9F 80..BF 80..BF
    // U+E000..U+FFFF          EE..EF 80..BF 80..BF
    // U+10000..U+3FFFF        F0 90..BF 80..BF 80..BF
    // U+40000..U+FFFFF        F1..F3 80..BF 80..BF 80..BF
    // U+100000..U+10FFFF      F4 80..8F 80..BF 80..BF

    // Check rules for 1 byte
    uint sum = freqs0[0xC0] + freqs0[0xC1];
    uint sum2 = 0;
    bool res = true;

    for (int i = 0xF5; i <= 0xFF; i++)
        sum += freqs0[i];

    if (sum != 0) {
        res = false;
        goto end;
    }

    // Check rules for first 2 bytes
    for (int i = 0; i < 256; i++) {
        // Exclude < 0xE0A0 || > 0xE0BF
        if ((i < 0xA0) || (i > 0xBF))
            sum += freqs1[0xE0 * 256 + i];

        // Exclude < 0xED80 || > 0xEDE9F
        if ((i < 0x80) || (i > 0x9F))
            sum += freqs1[0xED * 256 + i];

        // Exclude < 0xF090 || > 0xF0BF
        if ((i < 0x90) || (i > 0xBF))
            sum += freqs1[0xF0 * 256 + i];

        // Exclude < 0xF480 || > 0xF48F
        if ((i < 0x80) || (i > 0x8F))
            sum += freqs1[0xF4 * 256 + i];

        if ((i < 0x80) || (i > 0xBF)) {
            // Exclude < 0x??80 || > 0x??BF with ?? in [C2..DF]
            for (int j = 0xC2; j <= 0xDF; j++)
                sum += freqs1[j * 256 + i];

            // Exclude < 0x??80 || > 0x??BF with ?? in [E1..EC]
            for (int j = 0xE1; j <= 0xEC; j++)
                sum += freqs1[j * 256 + i];

            // Exclude < 0x??80 || > 0x??BF with ?? in [F1..F3]
            sum += freqs1[0xF1 * 256 + i];
            sum += freqs1[0xF2 * 256 + i];
            sum += freqs1[0xF3 * 256 + i];

            // Exclude < 0xEE80 || > 0xEEBF
            sum += freqs1[0xEE * 256 + i];

            // Exclude < 0xEF80 || > 0xEFBF
            sum += freqs1[0xEF * 256 + i];
        }
        else {
            // Count non-primary bytes
            sum2 += freqs0[i];
        }

        if (sum != 0) {
            res = false;
            break;
        }
    }

end:
    // Ad-hoc threshold
    res &= (sum2 >= uint(count / 8));
    return res == true ? TextCodec::MASK_NOT_TEXT | byte(Global::UTF8) : TextCodec::MASK_NOT_TEXT;
}


TextCodec::TextCodec()
{
    _delegate = nullptr;
    _pCtx = nullptr;
    _encodingType = 0;
    _bsVersion = 7;
    setEncodingType(1);
}

TextCodec::TextCodec(Context& ctx)
{
    _delegate = nullptr;
    _pCtx = &ctx;
    _encodingType = 0;
    _bsVersion = ctx.getInt("bsVersion", 7);
    setEncodingType(ctx.getInt("textcodec", 1));
}

void TextCodec::setEncodingType(int encodingType)
{
    if ((_delegate != nullptr) && (_encodingType == encodingType))
        return;

    delete _delegate;
    _encodingType = encodingType;
    _delegate = (encodingType == 1) ?
        static_cast<Transform<byte>*>(_pCtx == nullptr ? static_cast<Transform<byte>*>(new TextCodec1()) : static_cast<Transform<byte>*>(new TextCodec1(*_pCtx))) :
        static_cast<Transform<byte>*>(_pCtx == nullptr ? static_cast<Transform<byte>*>(new TextCodec2()) : static_cast<Transform<byte>*>(new TextCodec2(*_pCtx)));
}

bool TextCodec::forward(SliceArray<byte>& input, SliceArray<byte>& output, int count)
{
    if (count == 0)
        return true;

    if ((count < MIN_BLOCK_SIZE) || (count > MAX_BLOCK_SIZE))
        return false;

    if (!SliceArray<byte>::isValid(input))
       throw invalid_argument("TextCodec: Invalid input block");

    if (!SliceArray<byte>::isValid(output))
        throw invalid_argument("TextCodec: Invalid output block");

    if (input._array == output._array)
        return false;

    const int dstIdx = output._index;
    const bool res = _delegate->forward(input, output, count);

    if ((res == true) && (_bsVersion >= 7)) {
        if (_encodingType == 1)
            output._array[dstIdx] &= ~TextCodec::MASK_TEXT_CODEC;
        else
            output._array[dstIdx] |= TextCodec::MASK_TEXT_CODEC;
    }

    return res;
}

bool TextCodec::inverse(SliceArray<byte>& input, SliceArray<byte>& output, int count)
{
    if (count == 0)
        return true;

    if (count > MAX_BLOCK_SIZE) // ! no min
        return false;

    if (!SliceArray<byte>::isValid(input))
        throw invalid_argument("TextCodec: Invalid input block");

    if (!SliceArray<byte>::isValid(output))
        throw invalid_argument("TextCodec: Invalid output block");

    if (input._array == output._array)
        return false;

    if ((count < 2) || (input._index + count > input._length))
        return false;

    // Since bsVersion 7, the transform header stores which TextCodec variant
    // produced the bitstream, so switch delegate before decoding payload bytes.
    if (_bsVersion >= 7) {
        const int encodingType = ((input._array[input._index] & TextCodec::MASK_TEXT_CODEC) == byte(0)) ? 1 : 2;
        setEncodingType(encodingType);
    }

    return _delegate->inverse(input, output, count);
}

TextCodec1::TextCodec1()
{
    _logHashSize = TextCodec::LOG_HASHES_SIZE;
    _dictSize = 1 << 13;
    _dictMap = nullptr;
    _dictList = nullptr;
    _hashMask = (1 << _logHashSize) - 1;
    _staticDictSize = TextCodec::STATIC_DICT_WORDS;
    _isCRLF = false;
    _escapes[0] = TextCodec::ESCAPE_TOKEN2;
    _escapes[1] = TextCodec::ESCAPE_TOKEN1;
    _pCtx = nullptr;
}

TextCodec1::TextCodec1(Context& ctx)
{
    // Actual block size
    const int blockSize = ctx.getInt("blockSize", 0);
    const int log = blockSize >= 8 ? max(min(Global::log2(uint32(blockSize / 8)), 26), 13) : 13;
    _logHashSize = ctx.getString("entropy") == "TPAQX" ? log + 1 : log;
    _dictSize = 1 << 13;
    _dictMap = nullptr;
    _dictList = nullptr;
    _hashMask = (1 << _logHashSize) - 1;
    _staticDictSize = TextCodec::STATIC_DICT_WORDS;
    _isCRLF = false;
    _escapes[0] = TextCodec::ESCAPE_TOKEN2;
    _escapes[1] = TextCodec::ESCAPE_TOKEN1;
    _pCtx = &ctx;
}

void TextCodec1::reset(int count)
{
    // Select an appropriate initial dictionary size
    const int allocatedSize = _dictSize;
    const int log = count < 1024 ? 13 : max(min(Global::log2(uint32(count / 128)), 18), 13);
    _dictSize = max(TextCodec::STATIC_DICT_WORDS + 2, 1 << log);
    const int mapSize = 1 << _logHashSize;

    if (_dictMap == nullptr)
        _dictMap = new DictEntry*[mapSize];

    for (int i = 0; i < mapSize; i++)
        _dictMap[i] = nullptr;

    if ((_dictList == nullptr) || (allocatedSize < _dictSize)) {
        DictEntry* newDict = new DictEntry[_dictSize];
        delete[] _dictList;
        _dictList = newDict;
#if __cplusplus >= 201103L
        memcpy(&_dictList[0], &TextCodec::STATIC_DICTIONARY[0], sizeof(TextCodec::STATIC_DICTIONARY));
#else
        for (int i = 0; i < TextCodec::STATIC_DICT_WORDS; i++)
            _dictList[i] = TextCodec::STATIC_DICTIONARY[i];
#endif

        // Add special entries at end of static dictionary
        _staticDictSize = TextCodec::STATIC_DICT_WORDS;
        _dictList[_staticDictSize] = DictEntry(&_escapes[0], 0, _staticDictSize, 1);
        _dictList[_staticDictSize + 1] = DictEntry(&_escapes[1], 0, _staticDictSize + 1, 1);
        _staticDictSize += 2;
    }

    for (int i = 0; i < _staticDictSize; i++)
        _dictMap[_dictList[i]._hash & _hashMask] = &_dictList[i];

    // Pre-allocate all dictionary entries
    for (int i = _staticDictSize; i < _dictSize; i++)
        _dictList[i] = DictEntry(nullptr, 0, i);
}

bool TextCodec1::forward(SliceArray<byte>& input, SliceArray<byte>& output, int count)
{
    if (output._length - output._index < getMaxEncodedLength(count))
        return false;

    const byte* src = &input._array[input._index];
    byte* dst = &output._array[output._index];
    int srcIdx = 0;
    int dstIdx = 0;

    if (_pCtx != nullptr) {
        Global::DataType dt = (Global::DataType) _pCtx->getInt("dataType", Global::UNDEFINED);

        // Filter out most types. Still check binaries which may contain significant parts of text
        if ((dt != Global::UNDEFINED) && (dt != Global::TEXT) && (dt != Global::BIN))
            return false;
    }

    uint freqs[256] = { 0 };
    byte mode = TextCodec::computeStats(&src[srcIdx], count, freqs, true);

    // Not text ?
    if ((mode & TextCodec::MASK_NOT_TEXT) != byte(0)) {
        if (_pCtx != nullptr)
            _pCtx->putInt("dataType", Global::DataType(mode & TextCodec::MASK_DT));

        return false;
    }

    if (_pCtx != nullptr)
       _pCtx->putInt("dataType", Global::TEXT);

    reset(count);
    const int srcEnd = count;
    const int dstEnd = getMaxEncodedLength(count);
    const int dstEnd4 = dstEnd - 4;
    int emitAnchor = 0; // never less than 0
    int words = _staticDictSize;

    // DOS encoded end of line (CR+LF) ?
    _isCRLF = int(mode & TextCodec::MASK_CRLF) != 0;
    dst[dstIdx++] = mode;
    bool res = true;

    while ((srcIdx < srcEnd) && (src[srcIdx] == TextCodec::SP)) {
        dst[dstIdx++] = TextCodec::SP;
        srcIdx++;
        emitAnchor++;
    }

    int delimAnchor = TextCodec::isText(src[srcIdx]) ? srcIdx - 1 : srcIdx; // previous delimiter
    uint h1 = TextCodec::HASH1;
    uint h2 = TextCodec::HASH1;

    while (srcIdx < srcEnd) {
        const byte cur = src[srcIdx];
        const int8 cType = TextCodec::getType(cur);

        if (cType == 0) {
            if (srcIdx - delimAnchor == 1) {
                h1 = uint(TextCodec::HASH1) * uint(TextCodec::HASH1) ^ uint(cur) * uint(TextCodec::HASH2);
                h2 = uint(TextCodec::HASH1) * uint(TextCodec::HASH1) ^ (uint(cur) ^ 0x20U) * uint(TextCodec::HASH2);
            }
            else {
                h1 = h1 * TextCodec::HASH1 ^ uint(cur) * TextCodec::HASH2;
                h2 = h2 * TextCodec::HASH1 ^ uint(cur) * TextCodec::HASH2;
            }

            srcIdx++;
            continue;
        }

        if ((srcIdx > delimAnchor + 2) && (cType > 0)) { // At least 2 letters
            const int length = srcIdx - delimAnchor - 1;

            if (length <= TextCodec::MAX_WORD_LENGTH) {
                // Check word in dictionary
                prefetchRead(&_dictMap[h1 & _hashMask]);
                DictEntry* pe = nullptr;
                DictEntry* pe1 = _dictMap[h1 & _hashMask];

                if ((pe1 != nullptr) && (pe1->_hash == h1) && ((pe1->_data >> 24) == length))
                    pe = pe1;
                else {
                    prefetchRead(&_dictMap[h2 & _hashMask]);
                    DictEntry* pe2 = _dictMap[h2 & _hashMask];

                    if ((pe2 != nullptr) && (pe2->_hash == h2) && ((pe2->_data >> 24) == length))
                        pe = pe2;
                }

                // Check for hash collisions
                if ((pe != nullptr) && (KANZI_UNLIKELY(!TextCodec::sameWords(&pe->_ptr[1], &src[delimAnchor + 2], length - 1))))
                    pe = nullptr;

                if (pe == nullptr) {
                    // Word not found in the dictionary or hash collision.
                    // Replace entry if not in static dictionary
                    if (((length > 3) || ((length == 3) && (words < TextCodec::THRESHOLD2))) && (pe1 == nullptr)) {
                        DictEntry* pe3 = &_dictList[words];

                        if ((pe3->_data & TextCodec::MASK_LENGTH) >= _staticDictSize) {
                            // Reuse old entry
                            _dictMap[pe3->_hash & _hashMask] = nullptr;
                            pe3->_ptr = &src[delimAnchor + 1];
                            pe3->_hash = h1;
                            pe3->_data = (length << 24) | words;
                        }

                        // Update hash map
                        _dictMap[h1 & _hashMask] = pe3;
                        words++;

                        // Dictionary full ? Expand or reset index to end of static dictionary
                        if (words >= _dictSize) {
                            if (expandDictionary() == false)
                                words = _staticDictSize;
                        }
                    }
                }
                else {
                    // Word found in the dictionary
                    // Skip space if only delimiter between 2 word references
                    if ((emitAnchor != delimAnchor) || (src[delimAnchor] != byte(' '))) {
                        const int dIdx = emitSymbols(&src[emitAnchor], &dst[dstIdx], delimAnchor + 1 - emitAnchor, dstEnd - dstIdx);

                        if (dIdx < 0) {
                            res = false;
                            break;
                        }

                        dstIdx += dIdx;
                    }

                    if (dstIdx >= dstEnd4) {
                        res = false;
                        break;
                    }

                    dst[dstIdx++] = (pe == pe1) ? TextCodec::ESCAPE_TOKEN1 : TextCodec::ESCAPE_TOKEN2;
                    dstIdx += emitWordIndex(&dst[dstIdx], pe->_data & TextCodec::MASK_LENGTH);
                    emitAnchor = delimAnchor + 1 + int(pe->_data >> 24);
                }
            }
        }

        // Reset delimiter position
        delimAnchor = srcIdx;
        srcIdx++;
    }

    if (res == true) {
        // Emit last symbols
        const int dIdx = emitSymbols(&src[emitAnchor], &dst[dstIdx], srcEnd - emitAnchor, dstEnd - dstIdx);

        if (dIdx < 0)
            res = false;
        else
            dstIdx += dIdx;

        res &= (srcIdx == srcEnd);
    }

    output._index += dstIdx;
    input._index += srcIdx;
    return res;
}

bool TextCodec1::expandDictionary()
{
    if (_dictSize >= TextCodec::MAX_DICT_SIZE)
        return false;

    DictEntry* newDict = new DictEntry[_dictSize * 2];
    memcpy(static_cast<void*>(&newDict[0]), &_dictList[0], sizeof(DictEntry) * _dictSize);

    for (int i = _dictSize; i < _dictSize * 2; i++)
        newDict[i] = DictEntry(nullptr, 0, i);

    delete[] _dictList;
    _dictList = newDict;

    // Reset map (values must point to addresses of new DictEntry items)
    for (int i = 0; i < _dictSize; i++) {
        _dictMap[_dictList[i]._hash & _hashMask] = &_dictList[i];
    }

    _dictSize <<= 1;
    return true;
}

int TextCodec1::emitSymbols(const byte src[], byte dst[], const int srcEnd, const int dstEnd) const
{
    int dstIdx = 0;

    for (int i = 0; i < srcEnd; i++) {
        if (dstIdx >= dstEnd)
            return -1;

// Work around incorrect warning by GCC 7.x.x with C++17
#ifdef __GNUC__
    #if (__GNUC__ == 7) && (__cplusplus > 201402L)
        #pragma GCC diagnostic push
        #pragma GCC diagnostic ignored "-Wswitch"
    #endif
#endif

        const byte cur = src[i];

        switch (cur) {
        case TextCodec::ESCAPE_TOKEN1:
        case TextCodec::ESCAPE_TOKEN2: {
            // Emit special word
            dst[dstIdx++] = TextCodec::ESCAPE_TOKEN1;
            const int idx = (cur == TextCodec::ESCAPE_TOKEN1) ? _staticDictSize - 1 : _staticDictSize - 2;
            int lenIdx = 1;

            if (idx >= TextCodec::THRESHOLD1)
                lenIdx = (idx >= TextCodec::THRESHOLD2) ? 3 : 2;

            if (dstIdx + lenIdx >= dstEnd)
                return -1;

            dstIdx += emitWordIndex(&dst[dstIdx], idx);
            break;
        }

        case TextCodec::CR:
            if (_isCRLF == false)
                dst[dstIdx++] = cur;

            break;

        default:
            dst[dstIdx++] = cur;
        }
    }

// Work around incorrect warning by GCC 7.x.x with C++17
#ifdef __GNUC__
    #if (__GNUC__ == 7) && (__cplusplus > 201402L)
        #pragma GCC diagnostic pop
    #endif
#endif

    return dstIdx;
}

int TextCodec1::emitWordIndex(byte dst[], int val)
{
    // Emit word index (varint 5 bits + 7 bits + 7 bits)
    if (val >= TextCodec::THRESHOLD1) {
        if (val >= TextCodec::THRESHOLD2) {
            dst[0] = byte(0xE0 | (val >> 14));
            dst[1] = byte(0x80 | (val >> 7));
            dst[2] = byte(0x7F & val);
            return 3;
        }

        dst[0] = byte(0x80 | (val >> 7));
        dst[1] = byte(0x7F & val);
        return 2;
    }

    dst[0] = byte(val);
    return 1;
}

bool TextCodec1::inverse(SliceArray<byte>& input, SliceArray<byte>& output, int count)
{
    if ((count < 2) || (input._index + count > input._length))
       return false;

    reset(output._length);
    const byte* src = &input._array[input._index];
    byte* dst = &output._array[output._index];
    _isCRLF = int(src[0] & TextCodec::MASK_CRLF) != 0;
    const bool isCRLF = _isCRLF;
    int srcIdx = 1;
    int dstIdx = 0;
    const int srcEnd = count;
    const int dstEnd = output._length - output._index;
    int delimAnchor = TextCodec::isText(src[srcIdx]) ? srcIdx - 1 : srcIdx; // previous delimiter
    int words = _staticDictSize;
    bool wordRun = false;
    bool res = true;

    while ((srcIdx < srcEnd) && (dstIdx < dstEnd)) {
        const byte cur = src[srcIdx];
        const int8 cType = TextCodec::getType(cur);

        if (cType == 0) {
            dst[dstIdx] = src[srcIdx];
            srcIdx++;
            dstIdx++;
            continue;
        }

        if ((srcIdx > delimAnchor + 3) && (cType > 0)) {
            const int length = srcIdx - delimAnchor - 1; // length > 2

            if (length <= TextCodec::MAX_WORD_LENGTH) {
                uint h1 = TextCodec::HASH1;
                h1 = h1 * TextCodec::HASH1 ^ uint(src[delimAnchor + 1]) * TextCodec::HASH2;
                h1 = h1 * TextCodec::HASH1 ^ uint(src[delimAnchor + 2]) * TextCodec::HASH2;

                for (int i = delimAnchor + 3; i < srcIdx; i++)
                    h1 = h1 * TextCodec::HASH1 ^ uint(src[i]) * TextCodec::HASH2;

                // Lookup word in dictionary
                DictEntry* pe = nullptr;
                DictEntry* pe1 = _dictMap[h1 & _hashMask];

                // A hash collision check is not needed here: insertion is only allowed when
                // the hash slot is null, so the candidate contents cannot affect the outcome.
                if ((pe1 != nullptr) && (pe1->_hash == h1) && ((pe1->_data >> 24) == length))
                    pe = pe1;

                if (pe == nullptr) {
                    // Word not found in the dictionary or hash collision.
                    // Replace entry if not in static dictionary
                    if (((length > 3) || (words < TextCodec::THRESHOLD2)) && (pe1 == nullptr)) {
                        DictEntry& e = _dictList[words];

                        if ((e._data & TextCodec::MASK_LENGTH) >= _staticDictSize) {
                            // Reuse old entry
                            _dictMap[e._hash & _hashMask] = nullptr;
                            e._ptr = &src[delimAnchor + 1];
                            e._hash = h1;
                            e._data = (length << 24) | words;
                        }

                        _dictMap[h1 & _hashMask] = &e;
                        words++;

                        // Dictionary full ? Expand or reset index to end of static dictionary
                        if (words >= _dictSize) {
                            if (expandDictionary() == false)
                                words = _staticDictSize;
                        }
                    }
                }
            }
        }

        srcIdx++;

        if ((cur == TextCodec::ESCAPE_TOKEN1) || (cur == TextCodec::ESCAPE_TOKEN2)) {
            // Word in dictionary
            // Read word index (varint 5 bits + 7 bits + 7 bits)
            if (srcIdx >= srcEnd) {
                res = false;
                break;
            }

            int idx = int(src[srcIdx++]);

            if (idx >= 128) {
                if (srcIdx >= srcEnd) {
                    res = false;
                    break;
                }

                const int idx2 = int(src[srcIdx++]);

                if (idx2 >= 128) {
                    if (srcIdx >= srcEnd) {
                        res = false;
                        break;
                    }

                    idx = ((idx & 0x1F) << 14) | ((idx2 & 0x7F) << 7) | int(src[srcIdx]);
                    srcIdx++;
                }
                else {
                    idx = ((idx & 0x7F) << 7) | idx2;
                }

                if (KANZI_UNLIKELY(idx >= _dictSize)) {
                    res = false;
                    break;
                }
            }

            const int length = (_dictList[idx]._data >> 24) & 0xFF;

            // Emit word
            if (length > 1) {
                // Add space if only delimiter between 2 words (not an escaped delimiter)
                if (wordRun == true)
                    dst[dstIdx++] = TextCodec::SP;

                // Regular word entry
                wordRun = true;
                delimAnchor = srcIdx;
            }
            else {
                if (KANZI_UNLIKELY(length == 0)) {
                   res = false;
                   break;
                }

                // Escape entry
                wordRun = false;
                delimAnchor = srcIdx - 1;
            }

            // Sanity check
            if (KANZI_UNLIKELY(dstIdx + length > dstEnd)) {
                res = false;
                break;
            }

            memcpy(&dst[dstIdx], _dictList[idx]._ptr, length);

            // Flip case of first character ?
            if (cur == TextCodec::ESCAPE_TOKEN2)
               dst[dstIdx] ^= byte(0x20);

            dstIdx += length;
        }
        else {
            wordRun = false;
            delimAnchor = srcIdx - 1;

            if ((isCRLF == true) && (cur == TextCodec::LF)) {
                dst[dstIdx++] = TextCodec::CR;

                if (dstIdx >= dstEnd) {
                    res = false;
                    break;
                }
            }

            dst[dstIdx++] = cur;
        }
    }

    output._index += dstIdx;
    input._index += srcIdx;
    return (res == true) && (srcIdx == srcEnd);
}

TextCodec2::TextCodec2()
{
    _logHashSize = TextCodec::LOG_HASHES_SIZE;
    _dictSize = 1 << 13;
    _dictMap = nullptr;
    _dictList = nullptr;
    _hashMask = (1 << _logHashSize) - 1;
    _staticDictSize = TextCodec::STATIC_DICT_WORDS;
    _isCRLF = false;
    _pCtx = nullptr;
    _bsVersion = 6;
}

TextCodec2::TextCodec2(Context& ctx)
{
    const int blockSize = ctx.getInt("blockSize", 0);
    const int log = blockSize >= 32 ? max(min(Global::log2(uint32(blockSize / 32)), 24), 13) : 13;
    _logHashSize = ctx.getString("entropy") == "TPAQX" ? log + 1 : log;
    _dictSize = 1 << 13;
    _dictMap = nullptr;
    _dictList = nullptr;
    _hashMask = (1 << _logHashSize) - 1;
    _staticDictSize = TextCodec::STATIC_DICT_WORDS;
    _isCRLF = false;
    _pCtx = &ctx;
    _bsVersion = ctx.getInt("bsVersion");
}

void TextCodec2::reset(int count)
{
    // Select an appropriate initial dictionary size
    const int allocatedSize = _dictSize;
    const int log = count < 1024 ? 13 : max(min(Global::log2(uint32(count / 128)), 18), 13);
    _dictSize = max(TextCodec::STATIC_DICT_WORDS, 1 << log);
    const int mapSize = 1 << _logHashSize;

    if (_dictMap == nullptr)
        _dictMap = new DictEntry*[mapSize];

    for (int i = 0; i < mapSize; i++)
        _dictMap[i] = nullptr;

    if ((_dictList == nullptr) || (allocatedSize < _dictSize)) {
        DictEntry* newDict = new DictEntry[_dictSize];
        delete[] _dictList;
        _dictList = newDict;
#if __cplusplus >= 201103L
        memcpy(&_dictList[0], &TextCodec::STATIC_DICTIONARY[0], sizeof(TextCodec::STATIC_DICTIONARY));
#else
        for (int i = 0; i < TextCodec::STATIC_DICT_WORDS; i++)
            _dictList[i] = TextCodec::STATIC_DICTIONARY[i];
#endif
    }

    for (int i = 0; i < _staticDictSize; i++)
        _dictMap[_dictList[i]._hash & _hashMask] = &_dictList[i];

    // Pre-allocate all dictionary entries
    for (int i = _staticDictSize; i < _dictSize; i++)
        _dictList[i] = DictEntry(nullptr, 0, i);
}

bool TextCodec2::forward(SliceArray<byte>& input, SliceArray<byte>& output, int count)
{
    if (output._length - output._index < getMaxEncodedLength(count))
        return false;

    const byte* src = &input._array[input._index];
    byte* dst = &output._array[output._index];

    if (_pCtx != nullptr) {
        Global::DataType dt = (Global::DataType) _pCtx->getInt("dataType", Global::UNDEFINED);

        // Filter out most types. Still check binaries which may contain significant parts of text
        if ((dt != Global::UNDEFINED) && (dt != Global::TEXT) && (dt != Global::BIN))
            return false;
    }

    uint freqs[256] = { 0 };
    byte mode = TextCodec::computeStats(&src[0], count, freqs, false);

    // Not text ?
    if ((mode & TextCodec::MASK_NOT_TEXT) != byte(0)) {
        if (_pCtx != nullptr)
            _pCtx->putInt("dataType", Global::DataType(mode & TextCodec::MASK_DT));

        return false;
    }

    if (_pCtx != nullptr)
       _pCtx->putInt("dataType", Global::TEXT);

    reset(count);
    const int srcEnd = count;
    const int dstEnd = getMaxEncodedLength(count);
    const int dstEnd3 = dstEnd - 3;
    int emitAnchor = 0;
    int words = _staticDictSize;

    // DOS encoded end of line (CR+LF) ?
    _isCRLF = (mode & TextCodec::MASK_CRLF) != byte(0);
    dst[0] = mode;
    bool res = true;
    int srcIdx = 0;
    int dstIdx = 1;

    while ((srcIdx < srcEnd) && (src[srcIdx] == TextCodec::SP)) {
        dst[dstIdx++] = TextCodec::SP;
        srcIdx++;
        emitAnchor++;
    }

    int delimAnchor = TextCodec::isText(src[srcIdx]) ? srcIdx - 1 : srcIdx; // previous delimiter
    uint h1 = TextCodec::HASH1;
    uint h2 = TextCodec::HASH1;

    while (srcIdx < srcEnd) {
        const byte cur = src[srcIdx];
        const int8 cType = TextCodec::getType(cur);

        if (cType == 0) {
            if (srcIdx - delimAnchor == 1) {
                h1 = uint(TextCodec::HASH1) * uint(TextCodec::HASH1) ^ uint(cur) * uint(TextCodec::HASH2);
                h2 = uint(TextCodec::HASH1) * uint(TextCodec::HASH1) ^ (uint(cur) ^ 0x20U) * uint(TextCodec::HASH2);
            }
            else {
                h1 = h1 * TextCodec::HASH1 ^ uint(cur) * TextCodec::HASH2;
                h2 = h2 * TextCodec::HASH1 ^ uint(cur) * TextCodec::HASH2;
            }

            srcIdx++;
            continue;
        }

        if ((srcIdx > delimAnchor + 2) && (cType > 0)) {
            const int length = srcIdx - delimAnchor - 1;

            if (length <= TextCodec::MAX_WORD_LENGTH) {
                // Check word in dictionary
                prefetchRead(&_dictMap[h1 & _hashMask]);
                DictEntry* pe = nullptr;
                DictEntry* pe1 = _dictMap[h1 & _hashMask];

                if ((pe1 != nullptr) && (pe1->_hash == h1) && ((pe1->_data >> 24) == length))
                    pe = pe1;
                else {
                    prefetchRead(&_dictMap[h2 & _hashMask]);
                    DictEntry* pe2 = _dictMap[h2 & _hashMask];

                    if ((pe2 != nullptr) && (pe2->_hash == h2) && ((pe2->_data >> 24) == length))
                        pe = pe2;
                }

                // Check for hash collisions
                if ((pe != nullptr) && (KANZI_UNLIKELY(!TextCodec::sameWords(&pe->_ptr[1], &src[delimAnchor + 2], length - 1))))
                    pe = nullptr;

                if (pe == nullptr) {
                    // Word not found in the dictionary or hash collision.
                    // Replace entry if not in static dictionary
                    if (((length > 3) || ((length == 3) && (words < TextCodec::THRESHOLD2))) && (pe1 == nullptr)) {
                        DictEntry* pe3 = &_dictList[words];

                        if ((pe3->_data & TextCodec::MASK_LENGTH) >= _staticDictSize) {
                            // Reuse old entry
                            _dictMap[pe3->_hash & _hashMask] = nullptr;
                            pe3->_ptr = &src[delimAnchor + 1];
                            pe3->_hash = h1;
                            pe3->_data = (length << 24) | words;
                        }

                        // Update hash map
                        _dictMap[h1 & _hashMask] = pe3;
                        words++;

                        // Dictionary full ? Expand or reset index to end of static dictionary
                        if (words >= _dictSize) {
                            if (expandDictionary() == false)
                                words = _staticDictSize;
                        }
                    }
                }
                else {
                    // Word found in the dictionary
                    // Skip space if only delimiter between 2 word references
                    if ((emitAnchor != delimAnchor) || (src[delimAnchor] != TextCodec::SP)) {
                        const int dIdx = emitSymbols(&src[emitAnchor], &dst[dstIdx], delimAnchor + 1 - emitAnchor, dstEnd - dstIdx);

                        if (dIdx < 0) {
                            res = false;
                            break;
                        }

                        dstIdx += dIdx;
                    }

                    if (dstIdx >= dstEnd3) {
                        res = false;
                        break;
                    }

                    // Case flip is encoded as 0x80
                    dst[dstIdx] = TextCodec::MASK_FLIP_CASE;
                    dstIdx += (pe == pe1 ? 0 : 1);
                    dstIdx += emitWordIndex(&dst[dstIdx], pe->_data & TextCodec::MASK_LENGTH);
                    emitAnchor = delimAnchor + 1 + (pe->_data >> 24);
                }
            }
        }

        // Reset delimiter position
        delimAnchor = srcIdx;
        srcIdx++;
    }

    if (res == true) {
        // Emit last symbols
        const int dIdx = emitSymbols(&src[emitAnchor], &dst[dstIdx], srcEnd - emitAnchor, dstEnd - dstIdx);

        if (dIdx < 0)
            res = false;
        else
            dstIdx += dIdx;

        res &= (srcIdx == srcEnd);
    }

    output._index += dstIdx;
    input._index += srcIdx;
    return res;
}

bool TextCodec2::expandDictionary()
{
    if (_dictSize >= TextCodec::MAX_DICT_SIZE)
        return false;

    DictEntry* newDict = new DictEntry[_dictSize * 2];
    memcpy(static_cast<void*>(&newDict[0]), &_dictList[0], sizeof(DictEntry) * _dictSize);

    for (int i = _dictSize; i < _dictSize * 2; i++)
        newDict[i] = DictEntry(nullptr, 0, i);

    delete[] _dictList;
    _dictList = newDict;

    // Reset map (values must point to addresses of new DictEntry items)
    for (int i = 0; i < _dictSize; i++) {
        _dictMap[_dictList[i]._hash & _hashMask] = &_dictList[i];
    }

    _dictSize <<= 1;
    return true;
}

int TextCodec2::emitSymbols(const byte src[], byte dst[], const int srcEnd, const int dstEnd) const
{
    // Work around incorrect warning by GCC 7.x.x with C++17
    #ifdef __GNUC__
        #if (__GNUC__ == 7) && (__cplusplus > 201402L)
            #pragma GCC diagnostic push
            #pragma GCC diagnostic ignored "-Wswitch"
        #endif
    #endif

    int dstIdx = 0;

    if (2 * srcEnd < dstEnd) {
        for (int i = 0; i < srcEnd; i++) {
            switch (src[i]) {
            case TextCodec::ESCAPE_TOKEN1:
                dst[dstIdx++] = TextCodec::ESCAPE_TOKEN1;
                dst[dstIdx++] = TextCodec::ESCAPE_TOKEN1;
                break;

            case TextCodec::CR:
                if (_isCRLF == false)
                    dst[dstIdx++] = src[i];

                break;

            default:
                dst[dstIdx] = TextCodec::ESCAPE_TOKEN1;
                dstIdx += int(src[i] >> 7);
                dst[dstIdx++] = src[i];
            }
        }
    }
    else {
        for (int i = 0; i < srcEnd; i++) {
            switch (src[i]) {
            case TextCodec::ESCAPE_TOKEN1:
                if (dstIdx >= dstEnd - 1)
                    return -1;

                dst[dstIdx++] = TextCodec::ESCAPE_TOKEN1;
                dst[dstIdx++] = TextCodec::ESCAPE_TOKEN1;
                break;

            case TextCodec::CR:
                if (_isCRLF == false) {
                    if (dstIdx >= dstEnd)
                        return -1;

                    dst[dstIdx++] = src[i];
                }

                break;

            default:
                if (src[i] >= kanzi::byte(128)) {
                    if (dstIdx >= dstEnd)
                        return -1;

                    dst[dstIdx++] = TextCodec::ESCAPE_TOKEN1;
                }

                if (dstIdx >= dstEnd)
                    return -1;

                dst[dstIdx++] = src[i];
            }
        }
    }

    // Work around incorrect warning by GCC 7.x.x with C++17
    #ifdef __GNUC__
        #if (__GNUC__ == 7) && (__cplusplus > 201402L)
            #pragma GCC diagnostic pop
        #endif
    #endif

    return dstIdx;
}

int TextCodec2::emitWordIndex(kanzi::byte dst[], int wIdx)
{
    // 0x80 is reserved to first symbol case flip
    if (wIdx < V7_INDEX_BASE2) {
        dst[0] = kanzi::byte(0x81 + wIdx);
        return 1;
    }

    if (wIdx < V7_INDEX_BASE3) {
        // Encode the rank relative to the one-byte range.
        wIdx -= V7_INDEX_BASE2;
        dst[0] = kanzi::byte(0xC0 | (wIdx >> 8));
        dst[1] = kanzi::byte(wIdx);
        return 2;
    }

    // Encode the rank relative to the one- and two-byte ranges.
    wIdx -= V7_INDEX_BASE3;
    dst[0] = kanzi::byte(0xF0 | (wIdx >> 16));
    dst[1] = kanzi::byte(wIdx >> 8);
    dst[2] = kanzi::byte(wIdx);
    return 3;
}

bool TextCodec2::inverse(SliceArray<kanzi::byte>& input, SliceArray<kanzi::byte>& output, int count)
{
    if ((count < 2) || (input._index + count > input._length))
        return false;

    reset(output._length);
    const kanzi::byte* src = &input._array[input._index];
    kanzi::byte* dst = &output._array[output._index];
    _isCRLF = (src[0] & TextCodec::MASK_CRLF) != kanzi::byte(0);
    const bool isCRLF = _isCRLF;
    int srcIdx = 1;
    int dstIdx = 0;
    const int srcEnd = count;
    const int dstEnd = output._length - output._index;
    int delimAnchor = TextCodec::isText(src[srcIdx]) ? srcIdx - 1 : srcIdx; // previous delimiter
    int words = _staticDictSize;
    bool wordRun = false;
    bool res = true;
    const bool oldEncoding = _bsVersion < 6;

    while ((srcIdx < srcEnd) && (dstIdx < dstEnd)) {
        kanzi::byte cur = src[srcIdx];
        const int8 cType = TextCodec::getType(cur);

        if (cType == 0) {
            dst[dstIdx] = src[srcIdx];
            srcIdx++;
            dstIdx++;
            continue;
        }

        if ((srcIdx > delimAnchor + 3) && (cType > 0)) {
            const int length = srcIdx - delimAnchor - 1; // length > 2

            if (length <= TextCodec::MAX_WORD_LENGTH) {
                uint h1 = TextCodec::HASH1;
                h1 = h1 * TextCodec::HASH1 ^ uint(src[delimAnchor + 1]) * TextCodec::HASH2;
                h1 = h1 * TextCodec::HASH1 ^ uint(src[delimAnchor + 2]) * TextCodec::HASH2;

                for (int i = delimAnchor + 3; i < srcIdx; i++)
                    h1 = h1 * TextCodec::HASH1 ^ uint(src[i]) * TextCodec::HASH2;

                // Lookup word in dictionary
                DictEntry* pe = nullptr;
                DictEntry* pe1 = _dictMap[h1 & _hashMask];

                // A hash collision check is not needed here: insertion is only allowed when
                // the hash slot is null, so the candidate contents cannot affect the outcome.
                if ((pe1 != nullptr) && (pe1->_hash == h1) && ((pe1->_data >> 24) == length))
                    pe = pe1;

                if (pe == nullptr) {
                    // Word not found in the dictionary or hash collision.
                    // Replace entry if not in static dictionary
                    if (((length > 3) || (words < TextCodec::THRESHOLD2)) && (pe1 == nullptr)) {
                        DictEntry& e = _dictList[words];

                        if ((e._data & TextCodec::MASK_LENGTH) >= _staticDictSize) {
                            // Reuse old entry
                            _dictMap[e._hash & _hashMask] = nullptr;
                            e._ptr = &src[delimAnchor + 1];
                            e._hash = h1;
                            e._data = (length << 24) | words;
                        }

                        _dictMap[h1 & _hashMask] = &e;
                        words++;

                        // Dictionary full ? Expand or reset index to end of static dictionary
                        if (words >= _dictSize) {
                            if (expandDictionary() == false)
                                words = _staticDictSize;
                        }
                    }
                }
            }
        }

        srcIdx++;
        kanzi::byte flipMask = kanzi::byte(0);

        if (cur >= TextCodec::MASK_80) {
            // Word in dictionary
            int idx;

            if (oldEncoding == true) {
                // Read word index (varint 5 bits + 7 bits + 7 bits)
                flipMask = cur & TextCodec::MASK_20;
                idx = int(cur & TextCodec::MASK_1F);

                if ((cur & TextCodec::MASK_40) != kanzi::byte(0)) {
                    if (KANZI_UNLIKELY(srcIdx >= srcEnd)) {
                        res = false;
                        break;
                    }

                    const int idx2 = int(src[srcIdx++]);

                    if (idx2 >= 128) {
                        if (KANZI_UNLIKELY(srcIdx >= srcEnd)) {
                            res = false;
                            break;
                        }

                        idx = (idx << 14) | ((idx2 & 0x7F) << 7) | int(src[srcIdx]);
                        srcIdx++;
                    }
                    else {
                        idx = (idx << 7) | idx2;
                    }

                    // Sanity check
                    if (KANZI_UNLIKELY(idx >= _dictSize)) {
                        res = false;
                        break;
                    }
                }
            }
            else {
                const bool rankedEncoding = _bsVersion >= 7;

                if (cur == TextCodec::MASK_80) {
                    // Flip first char case
                    flipMask = TextCodec::MASK_20;

                    if (KANZI_UNLIKELY(srcIdx >= srcEnd)) {
                        res = false;
                        break;
                    }

                    cur = src[srcIdx++];
                }

                // Read word index
                // 10xxxxxx => 1 byte
                // 110xxxxx => 2 bytes
                // 1111xxxx => 3 bytes
                idx = int(cur) & 0x7F;
                const bool oneByte = idx < 64;

                if (idx >= 64) {
                    const bool threeBytes = idx >= 112;

                    if (threeBytes) {
                        if (KANZI_UNLIKELY(srcEnd - srcIdx < 2)) {
                            res = false;
                            break;
                        }

                        idx = ((idx & 0x0F) << 16) | (int(src[srcIdx]) << 8) | int(src[srcIdx + 1]);
                        srcIdx += 2;
                    }
                    else {
                        if (KANZI_UNLIKELY(srcIdx >= srcEnd)) {
                            res = false;
                            break;
                        }

                        idx = ((idx & 0x1F) << 8) | int(src[srcIdx]);
                        srcIdx++;
                    }

                    if (rankedEncoding == true) {
                        // Add the rank of the shorter forms. This prevents
                        // zero or negative dictionary indexes for malformed
                        // multi-byte encodings such as C0 00 or F0 00 00.
                        idx += threeBytes ? V7_INDEX_BASE3 : V7_INDEX_BASE2;

                        if (KANZI_UNLIKELY(idx >= _dictSize)) {
                            res = false;
                            break;
                        }
                    }
                    else if (KANZI_UNLIKELY(idx > _dictSize)) {
                        // V6 uses the original one-based dictionary index.
                        res = false;
                        break;
                    }
                }

                if (KANZI_UNLIKELY(idx == 0)) {
                    res = false;
                    break;
                }

                idx -= rankedEncoding ? int(oneByte) : 1;
            }

            const int length = (_dictList[idx]._data >> 24) & 0xFF;

            // Emit word
            if (length > 1) {
                // Add space if only delimiter between 2 words (not an escaped delimiter)
                if (wordRun == true)
                    dst[dstIdx++] = TextCodec::SP;

                // Regular word entry
                wordRun = true;
                delimAnchor = srcIdx;
            }
            else {
                if (KANZI_UNLIKELY(length == 0)) {
                   res = false;
                   break;
                }

                // Escape entry
                wordRun = false;
                delimAnchor = srcIdx - 1;
            }

            // Sanity check
            if (KANZI_UNLIKELY(dstIdx + length > dstEnd)) {
                res = false;
                break;
            }

            memcpy(&dst[dstIdx], _dictList[idx]._ptr, length);

            // Flip case of first character ?
            dst[dstIdx] ^= flipMask;
            dstIdx += length;
        }
        else {
            if (cur == TextCodec::ESCAPE_TOKEN1) {
                if (KANZI_UNLIKELY(srcIdx >= srcEnd)) {
                    res = false;
                    break;
                }

                dst[dstIdx++] = src[srcIdx++];
            }
            else {
                if ((isCRLF == true) && (cur == TextCodec::LF)) {
                    dst[dstIdx++] = TextCodec::CR;

                    if (KANZI_UNLIKELY(dstIdx >= dstEnd)) {
                        res = false;
                        break;
                    }
                }

                dst[dstIdx++] = cur;
            }

            wordRun = false;
            delimAnchor = srcIdx - 1;
        }
    }

    output._index += dstIdx;
    input._index += srcIdx;
    return (res == true) && (srcIdx == srcEnd);
}
