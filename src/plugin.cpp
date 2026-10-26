#include <D2RLPlugin/api.h>

#include <Windows.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <charconv>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <iterator>
#include <limits>
#include <string>
#include <string_view>
#include <system_error>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace {

constexpr char PluginId[] = "rune-gem-quick-upgrade";
constexpr std::uint32_t ModifierGenericKeyBit = 1U << 0U;
constexpr std::uint32_t ModifierLeftKeyBit = 1U << 1U;
constexpr std::uint32_t ModifierRightKeyBit = 1U << 2U;
constexpr std::uint32_t MaxRecipeInputs = 64;
constexpr std::uint32_t MaxRecipeOutputs = 64;
constexpr std::uint32_t MaxInputQuantity = 4096;
constexpr std::uint64_t AdvancedStashSlotInputRva = 0x2CF760;
constexpr std::uint64_t AdvancedStashSlotVtableRva = 0x1CF4678;
constexpr std::uint64_t AdvancedStashSlotCounterGetterRva = 0x2CE900;
constexpr std::uint64_t NativeGetUnitIdRva = 0x34A330;
constexpr std::uint64_t AdvancedStashCounterByteRva = 0x46D9A0;
constexpr std::uint64_t AdvancedStashCounterByteWitnessRva = 0x46D9A8;
constexpr std::uint64_t FindAdvancedStashSlotByClassRva = 0x46D8F0;
constexpr std::uint64_t ItemWidgetInputDispatcherRva = 0x2A98B0;
constexpr std::uint64_t ConsumeCurrentInputRva = 0x843FC0;
constexpr std::uint64_t AdvancedStashBatchWithdrawalRva = 0x1599A0;
constexpr std::uint64_t AdvancedStashSingleWithdrawalRva = 0x159B30;
constexpr std::uint64_t NativeGetLocalDataContextRva = 0x08B2D0;
constexpr std::uint64_t NativeGetLocalPlayerRva = 0x09A480;
constexpr std::uint64_t NativeGetItemCodeRva = 0x36EF50;
constexpr std::uint64_t InventoryItemWidgetDrawRva = 0x2C3390;
constexpr std::uint64_t InventorySlotWidgetDrawRva = 0x2A7170;
constexpr std::uint64_t InventoryItemWidgetVtableRva = 0x1CF3800;
constexpr std::uint64_t InventorySlotWidgetVtableRva = 0x1CF3F20;
constexpr std::uint64_t NativeGetAdvancedStashDestinationRva = 0x46DA50;
constexpr std::uint64_t NativeTransferItemToInventoryPageRva = 0x15F8B0;
constexpr std::uint64_t NativeFinishInventoryInteractionRva = 0x1A0780;
constexpr std::uint64_t NativeFindAdvancedStashSlotByClassRva = 0x46D8F0;
constexpr std::uint64_t NativeAttachSoundRva = 0x491960;
constexpr std::uintptr_t NativeItemDataPointerOffset = 0x10;
// The D2R item-data quality enum is the first field; ItemDataFlags follows at 0x18.
constexpr std::uintptr_t NativeItemQualityOffset = 0x00;
constexpr std::uintptr_t NativeItemFlagsOffset = 0x18;
constexpr std::uint32_t NativeItemIdentifiedMask = 0x00000010U;
static_assert(static_cast<std::uint32_t>(D2RL::Items::Quality::Normal) == 2U);
constexpr std::uint8_t AdvancedStashCubeDestination = 2;
// Native Advanced Stash withdrawal destinations: 0=cursor, 1=inventory, 2=Cube, 3=belt.
constexpr std::uint8_t AdvancedStashInventoryDestination = 1;
constexpr std::uint8_t AdvancedStashPage = 4;
constexpr std::uint8_t MainInventoryPage = 0;
constexpr std::uint8_t RecipeInputCount = 3;
constexpr std::uint32_t MaximumWithdrawalPolls = 300;
constexpr std::size_t MaximumConfigBytes = 16384;
constexpr std::size_t MaximumTemporarySpriteCodes = MaxRecipeInputs + MaxRecipeOutputs;
constexpr std::uint32_t HoradricCubeCode = D2RL::Items::MakeItemCode("box");
constexpr std::uint32_t RejuvenationPotionCode = D2RL::Items::MakeItemCode("rvs");
constexpr std::uint32_t FullRejuvenationPotionCode = D2RL::Items::MakeItemCode("rvl");
constexpr char DefaultConfig[] =
    "# Add exact recipe descriptions from the active mod's cubemain.txt.\n"
    "# Custom recipes require one output and stackable ingredients and output.\n"
    "included-recipes = []\n"
    "allow_without_cube = false\n";
constexpr std::uint64_t AdvancedStashRightClickCommand = 0x02D5172CAC7C938CULL;
constexpr std::uint64_t InventoryItemRendererRva = 0x2A72D0;
constexpr std::array<std::uint8_t, 32> AdvancedStashSlotInputExpected{
    0x48, 0x89, 0x5C, 0x24, 0x18, 0x48, 0x89, 0x54,
    0x24, 0x10, 0x55, 0x56, 0x57, 0x41, 0x56, 0x41,
    0x57, 0x48, 0x83, 0xEC, 0x30, 0x48, 0x8B, 0xD9,
    0xE8, 0x53, 0xBB, 0xDB, 0xFF, 0x8B, 0xC8, 0xE8,
};
constexpr std::array<std::uint8_t, 8> AdvancedStashSlotCounterGetterExpected{
    0x48, 0x8B, 0x81, 0x08, 0x06, 0x00, 0x00, 0xC3,
};
constexpr std::array<std::uint8_t, 32> NativeGetUnitIdExpected{
    0x48, 0x83, 0xEC, 0x28, 0x48, 0x85, 0xC9, 0x75,
    0x1D, 0x88, 0x4C, 0x24, 0x30, 0x48, 0x8D, 0x4C,
    0x24, 0x30, 0xE8, 0x39, 0xCA, 0xFF, 0xFF, 0x84,
    0xC0, 0x74, 0x01, 0xCC, 0xB8, 0xFF, 0xFF, 0xFF,
};
constexpr std::array<std::uint8_t, 12> AdvancedStashCounterByteExpected{
    0x48, 0x8B, 0x41, 0x10, 0x0F, 0xB6, 0x80, 0x9C,
    0x00, 0x00, 0x00, 0xC3,
};
constexpr std::array<std::uint8_t, 32> AdvancedStashBatchWithdrawalExpected{
    0x48, 0x89, 0x5C, 0x24, 0x08, 0x48, 0x89, 0x6C,
    0x24, 0x10, 0x48, 0x89, 0x74, 0x24, 0x18, 0x57,
    0x48, 0x83, 0xEC, 0x40, 0x48, 0x8B, 0xC2, 0x41,
    0x0F, 0xB6, 0xE8, 0x48, 0x8B, 0xD9, 0x41, 0x0F,
};
constexpr std::array<std::uint8_t, 32> NativeGetLocalDataContextExpected{
    0x8B, 0x05, 0x2E, 0x84, 0x99, 0x02, 0xC3, 0xCC,
    0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC,
    0x8B, 0x05, 0x76, 0x84, 0x99, 0x02, 0xC3, 0xCC,
    0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC,
};
constexpr std::array<std::uint8_t, 32> NativeGetLocalPlayerExpected{
    0x48, 0x89, 0x5C, 0x24, 0x08, 0x57, 0x48, 0x83,
    0xEC, 0x20, 0x83, 0xF9, 0x08, 0x0F, 0x83, 0x85,
    0x00, 0x00, 0x00, 0x8B, 0xD9, 0x48, 0x89, 0x5C,
    0x24, 0x38, 0x48, 0x83, 0xFB, 0x08, 0x72, 0x19,
};
constexpr std::array<std::uint8_t, 32> NativeGetItemCodeExpected{
    0x48, 0x89, 0x5C, 0x24, 0x10, 0x57, 0x48, 0x83,
    0xEC, 0x20, 0x48, 0x8B, 0xF9, 0x48, 0x85, 0xC9,
    0x75, 0x13, 0x88, 0x4C, 0x24, 0x30, 0x48, 0x8D,
    0x4C, 0x24, 0x30, 0xE8, 0x80, 0x83, 0xFF, 0xFF,
};
constexpr std::array<std::uint8_t, 32> NativeGetAdvancedStashDestinationExpected{
    0x40, 0x53, 0x48, 0x83, 0xEC, 0x20, 0x48, 0x85,
    0xC9, 0x75, 0x1B, 0x88, 0x4C, 0x24, 0x30, 0x48,
    0x8D, 0x4C, 0x24, 0x30, 0xE8, 0xC7, 0xF3, 0xFF,
    0xFF, 0x84, 0xC0, 0x74, 0x01, 0xCC, 0x33, 0xC0,
};
constexpr std::array<std::uint8_t, 32> NativeTransferItemToInventoryPageExpected{
    0x40, 0x55, 0x53, 0x56, 0x57, 0x41, 0x54, 0x41,
    0x55, 0x41, 0x56, 0x41, 0x57, 0x48, 0x8D, 0xAC,
    0x24, 0x08, 0xFF, 0xFF, 0xFF, 0x48, 0x81, 0xEC,
    0xF8, 0x01, 0x00, 0x00, 0x48, 0x8B, 0x05, 0xF5,
};
constexpr std::array<std::uint8_t, 32> NativeFinishInventoryInteractionExpected{
    0x40, 0x55, 0x56, 0x57, 0x41, 0x56, 0x41, 0x57,
    0x48, 0x8D, 0x6C, 0x24, 0xD1, 0x48, 0x81, 0xEC,
    0x00, 0x01, 0x00, 0x00, 0x48, 0x8B, 0x05, 0x2D,
    0xAB, 0x82, 0x02, 0x48, 0x33, 0xC4, 0x48, 0x89,
};
// Captured from the active D2RLoader process. The live fingerprint checks below
// keep this build-specific native path disabled when the game layout differs.
constexpr std::array<std::uint8_t, 32> NativeFindAdvancedStashSlotByClassExpected{
    0x48, 0x89, 0x5C, 0x24, 0x08, 0x57, 0x48, 0x83,
    0xEC, 0x20, 0x8B, 0xFA, 0x48, 0x8B, 0xD9, 0xBA,
    0xD1, 0x00, 0x00, 0x00, 0xE8, 0xA7, 0x78, 0xEC,
    0xFF, 0x85, 0xC0, 0x75, 0x13, 0x48, 0x8D, 0x4C,
};
constexpr std::array<std::uint8_t, 15> NativeAttachSoundExpected{
    0x48, 0x89, 0x5C, 0x24, 0x08, 0x48, 0x89, 0x74,
    0x24, 0x18, 0x57, 0x48, 0x83, 0xEC, 0x20,
};
constexpr std::array<std::uint8_t, 32> ItemWidgetInputDispatcherExpected{
    0x40, 0x55, 0x53, 0x57, 0x41, 0x56, 0x41, 0x57,
    0x48, 0x8D, 0xAC, 0x24, 0xC0, 0xFD, 0xFF, 0xFF,
    0x48, 0x81, 0xEC, 0x40, 0x03, 0x00, 0x00, 0x48,
    0x8B, 0x05, 0xFA, 0x19, 0x72, 0x02, 0x48, 0x33,
};
constexpr std::array<std::uint8_t, 32> ConsumeCurrentInputExpected{
    0x48, 0x8B, 0x05, 0xA9, 0xC1, 0xBF, 0x02, 0x48,
    0x85, 0xC0, 0x74, 0x07, 0xC6, 0x80, 0xB8, 0x00,
    0x00, 0x00, 0x01, 0xC3, 0xCC, 0xCC, 0xCC, 0xCC,
    0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC,
};
constexpr std::array<std::uint8_t, 32> InventoryItemRendererExpected{
    0x48, 0x85, 0xD2, 0x0F, 0x84, 0xD7, 0x00, 0x00,
    0x00, 0x56, 0x57, 0x48, 0x83, 0xEC, 0x68, 0x48,
    0x8B, 0xF9, 0x48, 0x89, 0x5C, 0x24, 0x60, 0x48,
    0x8B, 0x49, 0x30, 0x48, 0x8B, 0xF2, 0x0F, 0x29,
};
constexpr std::array<std::uint8_t, 32> InventoryItemWidgetDrawExpected{
    0x48, 0x89, 0x5C, 0x24, 0x08, 0x57, 0x48, 0x83,
    0xEC, 0x20, 0x48, 0x8B, 0xF9, 0xE8, 0xCE, 0x3D,
    0xFE, 0xFF, 0xE8, 0x99, 0x2C, 0xF3, 0xFF, 0x48,
    0x8B, 0xD8, 0x48, 0x85, 0xC0, 0x74, 0x3D, 0x80,
};
constexpr std::array<std::uint8_t, 32> InventorySlotWidgetDrawExpected{
    0x40, 0x53, 0x48, 0x83, 0xEC, 0x20, 0xE8, 0xC5,
    0xEE, 0xF4, 0xFF, 0x48, 0x8B, 0xD8, 0x48, 0x85,
    0xC0, 0x74, 0x30, 0x33, 0xD2, 0x48, 0x8B, 0xC8,
    0xE8, 0x73, 0xC9, 0x5C, 0x00, 0xE8, 0x6E, 0x7F,
};

using AdvancedStashSlotInputHandler = void(__fastcall*)(void*, std::uintptr_t) noexcept;
using AdvancedStashSlotCounterGetter = void*(__fastcall*)(void*) noexcept;
using NativeGetUnitId = std::int32_t(__fastcall*)(void*) noexcept;
using AdvancedStashCounterByte = std::uint8_t(__fastcall*)(void*) noexcept;
using NativeGetLocalDataContext = std::int32_t(__fastcall*)() noexcept;
using NativeGetLocalPlayer = void*(__fastcall*)(std::int32_t) noexcept;
using NativeGetItemCode = std::uint32_t(__fastcall*)(void*) noexcept;
using NativeGetAdvancedStashDestination = void*(__fastcall*)(void*) noexcept;
using NativeBatchWithdrawal = void(__fastcall*)(void*, void*, std::uint8_t, std::uint8_t) noexcept;
using NativeTransferItemToInventoryPage = bool(__fastcall*)(void*, void*, std::uint8_t, std::uint8_t, bool, void*) noexcept;
using NativeFinishInventoryInteraction = void(__fastcall*)(std::int32_t, void*, std::int32_t, std::int32_t, bool) noexcept;
using NativeFindAdvancedStashSlotByClass = void*(__fastcall*)(void*, std::int32_t) noexcept;
using NativeAttachSound = void(__fastcall*)(void*, std::uint32_t, void*) noexcept;
using ItemWidgetInputHandler = void(__fastcall*)(void*, const void*) noexcept;
using CurrentInputConsumer = void(__fastcall*)() noexcept;
using InventoryItemRenderer = void(__fastcall*)(void*, void*) noexcept;
using InventoryItemWidgetDraw = void(__fastcall*)(void*) noexcept;
using InventorySlotWidgetDraw = void(__fastcall*)(void*) noexcept;

constexpr D2RL::PluginInfo PluginInfo {
    .infoSize    = D2RL::PluginInfoSize,
    .abiVersion  = D2RL_PLUGIN_ABI_VERSION,
    .id          = PluginId,
    .name        = "Rune Gem Quick Upgrade",
    .version     = "0.1.71",
    .author      = "MadMike",
    .description = "Upgrade with Shift + right-click; downgrade with Alt + right-click.",
    .flags       = D2RL::PluginFlags::Client | D2RL::PluginFlags::NativeHooks,
};

const D2RL::InventoryService* InventoryService{};
const D2RL::ItemService* ItemService{};
const D2RL::ThreadService* ThreadService{};
const D2RL::InputService* InputApiService{};
D2RL::Input::KeyListenerHandle ModifierKeyListener{D2RL::Input::InvalidKeyListenerHandle};
const D2RL::PluginContext* LoadContext{};
std::atomic_bool RequestInFlight{};
std::atomic_bool HideTemporaryInventoryWidgets{};
std::atomic_bool ModifierKeyListenerActive{};
std::atomic_uint32_t AltKeyState{};
std::atomic_uint32_t ShiftKeyState{};
bool AllowWithoutCube{};
enum class SlotAction : std::uint8_t {
    None,
    Upgrade,
    Downgrade,
};

thread_local SlotAction ActiveSlotAction{SlotAction::None};
thread_local void* ActiveInputWidget{};
AdvancedStashSlotInputHandler OriginalAdvancedStashSlotInputHandler{};
ItemWidgetInputHandler OriginalItemWidgetInputHandler{};
CurrentInputConsumer OriginalCurrentInputConsumer{};
InventoryItemRenderer OriginalInventoryItemRenderer{};
InventoryItemWidgetDraw OriginalInventoryItemWidgetDraw{};
InventorySlotWidgetDraw OriginalInventorySlotWidgetDraw{};
std::array<std::atomic<std::uint32_t>, MaximumTemporarySpriteCodes> TemporarySpriteCodes{};
std::atomic<std::uint32_t> TemporarySpriteCodeCount{};
AdvancedStashSlotCounterGetter GetAdvancedStashSlotCounterItem{};
NativeGetUnitId GetNativeUnitId{};
AdvancedStashCounterByte GetAdvancedStashCounterByte{};
NativeGetLocalDataContext GetLocalDataContext{};
NativeGetLocalPlayer GetLocalPlayer{};
NativeGetItemCode GetNativeItemCode{};
NativeGetAdvancedStashDestination GetAdvancedStashDestination{};
NativeBatchWithdrawal RequestAdvancedStashBatchWithdrawal{};
NativeTransferItemToInventoryPage TransferItemToInventoryPage{};
NativeFinishInventoryInteraction FinishInventoryInteraction{};
NativeFindAdvancedStashSlotByClass FindAdvancedStashSlotByClass{};
NativeAttachSound AttachSound{};
std::uintptr_t MainImageBase{};

struct Ingredient {
    std::uint32_t code{};
    std::uint32_t quantity{};
};

struct Recipe {
    std::vector<Ingredient> inputs;
    std::vector<std::uint32_t> outputs;
    std::uint32_t sourceRow{};
    std::uint32_t totalInputQuantity{};
};

struct CraftingSlotLayout {
    std::string itemCode;
    std::int32_t x{};
    std::int32_t y{};
    std::int32_t width{};
    std::int32_t height{};
};

struct DefaultCraftingSlotLayout {
    std::string_view itemCode;
    std::int32_t x{};
    std::int32_t y{};
    std::int32_t width{};
    std::int32_t height{};
};

// Built-in coordinates for the supported Advanced Stash material-slot layout.
constexpr std::array<DefaultCraftingSlotLayout, 91> DefaultCraftingSlotLayouts{{
    // Gems
    { "gcw", 164, 232, 98, 98 }, { "gfw", 164, 330, 98, 98 }, { "gsw", 164, 430, 98, 98 },
    { "glw", 164, 530, 98, 98 }, { "gpw", 164, 630, 98, 98 }, { "gcg", 286, 232, 98, 98 },
    { "gfg", 286, 330, 98, 98 }, { "gsg", 286, 430, 98, 98 }, { "glg", 286, 530, 98, 98 },
    { "gpg", 286, 630, 98, 98 }, { "gcr", 408, 232, 98, 98 }, { "gfr", 408, 330, 98, 98 },
    { "gsr", 408, 430, 98, 98 }, { "glr", 408, 530, 98, 98 }, { "gpr", 408, 630, 98, 98 },
    { "gcy", 530, 232, 98, 98 }, { "gfy", 530, 330, 98, 98 }, { "gsy", 530, 430, 98, 98 },
    { "gly", 530, 530, 98, 98 }, { "gpy", 530, 630, 98, 98 }, { "gcv", 652, 232, 98, 98 },
    { "gfv", 652, 330, 98, 98 }, { "gsv", 652, 430, 98, 98 }, { "gzv", 652, 530, 98, 98 },
    { "gpv", 652, 630, 98, 98 }, { "gcb", 774, 232, 98, 98 }, { "gfb", 774, 330, 98, 98 },
    { "gsb", 774, 430, 98, 98 }, { "glb", 774, 530, 98, 98 }, { "gpb", 774, 630, 98, 98 },
    { "skc", 896, 232, 98, 98 }, { "skf", 896, 330, 98, 98 }, { "sku", 896, 430, 98, 98 },
    { "skl", 896, 530, 98, 98 }, { "skz", 896, 630, 98, 98 },
    // Other materials
    { "pk1", 102, 232, 98, 196 }, { "pk2", 206, 232, 98, 196 }, { "pk3", 310, 232, 98, 196 },
    { "ua1", 546, 232, 98, 196 }, { "ua2", 650, 232, 98, 196 }, { "ua3", 754, 232, 98, 196 },
    { "ua4", 858, 232, 98, 196 }, { "ua5", 962, 232, 98, 196 }, { "dhn", 102, 460, 98, 98 },
    { "bey", 206, 460, 98, 98 }, { "mbr", 310, 460, 98, 98 }, { "xa1", 546, 460, 98, 98 },
    { "xa2", 650, 460, 98, 98 }, { "xa3", 754, 460, 98, 98 }, { "xa4", 858, 460, 98, 98 },
    { "xa5", 962, 460, 98, 98 }, { "rvs", 102, 596, 98, 98 }, { "rvl", 206, 596, 98, 98 },
    { "toa", 540, 596, 98, 98 }, { "tes", 646, 596, 98, 98 }, { "ceh", 750, 596, 98, 98 },
    { "bet", 854, 596, 98, 98 }, { "fed", 958, 596, 98, 98 },
    // Runes
    { "r01", 114, 268, 98, 98 }, { "r02", 218, 272, 98, 98 }, { "r03", 322, 270, 98, 98 },
    { "r04", 426, 270, 98, 98 }, { "r05", 530, 270, 98, 98 }, { "r06", 634, 270, 98, 98 },
    { "r07", 738, 270, 98, 98 }, { "r08", 842, 270, 98, 98 }, { "r09", 946, 270, 98, 98 },
    { "r10", 114, 374, 98, 98 }, { "r11", 218, 374, 98, 98 }, { "r12", 322, 374, 98, 98 },
    { "r13", 426, 374, 98, 98 }, { "r14", 530, 374, 98, 98 }, { "r15", 634, 374, 98, 98 },
    { "r16", 738, 374, 98, 98 }, { "r17", 842, 374, 98, 98 }, { "r18", 946, 374, 98, 98 },
    { "r19", 114, 478, 98, 98 }, { "r20", 218, 478, 98, 98 }, { "r21", 322, 478, 98, 98 },
    { "r22", 426, 478, 98, 98 }, { "r23", 530, 478, 98, 98 }, { "r24", 634, 478, 98, 98 },
    { "r25", 738, 478, 98, 98 }, { "r26", 842, 478, 98, 98 }, { "r27", 946, 478, 98, 98 },
    { "r28", 114, 582, 98, 98 }, { "r29", 218, 582, 98, 98 }, { "r30", 842, 582, 98, 98 },
    { "r31", 946, 582, 98, 98 }, { "r32", 114, 688, 98, 98 }, { "r33", 946, 684, 98, 98 },
}};

enum class UpgradeKind : std::uint8_t {
    Gem,
    Rune,
};

struct ItemProgression {
    UpgradeKind kind{};
    std::uint32_t rank{};
};

std::vector<Recipe> Recipes;
std::unordered_set<std::string> IncludedRecipeNames;
std::vector<CraftingSlotLayout> CraftingSlotLayouts;
std::unordered_map<std::uint32_t, std::int32_t> ItemClassByCode;
std::unordered_set<std::uint32_t> AdvancedStashStackableItemCodes;
std::unordered_set<std::uint32_t> GemItemCodes;
std::unordered_set<std::uint32_t> RuneItemCodes;
std::uint16_t GemUpgradeSoundId{};
std::atomic_bool LoggedGemUpgradeSoundUnavailable{};

struct Table {
    std::vector<std::string> headers;
    std::vector<std::vector<std::string>> rows;
};

struct PendingSlotUpgrade {
    void* widget{};
    D2RL::PlayerHandle player{D2RL::InvalidPlayerHandle};
    std::uint32_t clickedCode{};
    std::uint32_t sourceRow{};
    std::uint32_t pollCount{};
    SlotAction action{SlotAction::None};
    bool withdrawalSent{};
    D2RL::Items::ItemContainer stagingContainer{D2RL::Items::ItemContainer::Cube};
    std::vector<D2RL::Items::ItemInfo> stagedItemsBefore;
    std::vector<Ingredient> transactionIngredients;
    std::vector<Ingredient> createdItems;
};

PendingSlotUpgrade PendingSlotRequest{};

auto Trim(std::string_view value) -> std::string {
    while (!value.empty() && std::isspace(static_cast<unsigned char>(value.front()))) {
        value.remove_prefix(1);
    }
    while (!value.empty() && std::isspace(static_cast<unsigned char>(value.back()))) {
        value.remove_suffix(1);
    }
    return std::string(value);
}

auto Lower(std::string value) -> std::string {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char character) {
        return static_cast<char>(std::tolower(character));
    });
    return value;
}

auto TrimView(std::string_view value) -> std::string_view {
    while (!value.empty() && std::isspace(static_cast<unsigned char>(value.front()))) {
        value.remove_prefix(1);
    }
    while (!value.empty() && std::isspace(static_cast<unsigned char>(value.back()))) {
        value.remove_suffix(1);
    }
    return value;
}

auto ReadTomlBoolean(std::string_view config, std::string_view key, bool& value) noexcept -> bool {
    std::size_t start = 0;
    while (start < config.size()) {
        const std::size_t end = config.find('\n', start);
        std::string_view line = config.substr(start,
            end == std::string_view::npos ? config.size() - start : end - start);
        const std::size_t comment = line.find('#');
        if (comment != std::string_view::npos) {
            line = line.substr(0, comment);
        }
        line = TrimView(line);
        const std::size_t equals = line.find('=');
        if (equals != std::string_view::npos && TrimView(line.substr(0, equals)) == key) {
            const std::string_view configuredValue = TrimView(line.substr(equals + 1));
            if (configuredValue == "true") {
                value = true;
                return true;
            }
            if (configuredValue == "false") {
                value = false;
                return true;
            }
            return false;
        }
        if (end == std::string_view::npos) {
            break;
        }
        start = end + 1;
    }
    return false;
}

auto ReadTomlStringArray(std::string_view config, std::string_view key,
    std::vector<std::string>& values, bool& found) -> bool {
    found = false;
    std::size_t lineStart = 0;
    while (lineStart < config.size()) {
        const std::size_t lineEnd = config.find('\n', lineStart);
        std::string_view line = config.substr(lineStart,
            lineEnd == std::string_view::npos ? config.size() - lineStart : lineEnd - lineStart);
        bool inString = false;
        bool escaped = false;
        for (std::size_t index = 0; index < line.size(); ++index) {
            if (inString) {
                if (escaped) {
                    escaped = false;
                } else if (line[index] == '\\') {
                    escaped = true;
                } else if (line[index] == '"') {
                    inString = false;
                }
            } else if (line[index] == '"') {
                inString = true;
            } else if (line[index] == '#') {
                line = line.substr(0, index);
                break;
            }
        }

        line = TrimView(line);
        const std::size_t equals = line.find('=');
        if (equals != std::string_view::npos && TrimView(line.substr(0, equals)) == key) {
            found = true;
            std::string_view array = TrimView(line.substr(equals + 1));
            if (array.empty() || array.front() != '[') {
                return false;
            }
            array.remove_prefix(1);
            std::vector<std::string> parsed;
            while (true) {
                array = TrimView(array);
                if (array.empty()) {
                    return false;
                }
                if (array.front() == ']') {
                    array.remove_prefix(1);
                    if (!TrimView(array).empty()) {
                        return false;
                    }
                    values = std::move(parsed);
                    return true;
                }
                if (array.front() != '"') {
                    return false;
                }
                array.remove_prefix(1);
                std::string value;
                bool closed = false;
                while (!array.empty()) {
                    const char character = array.front();
                    array.remove_prefix(1);
                    if (character == '"') {
                        closed = true;
                        break;
                    }
                    if (character == '\\') {
                        if (array.empty()) {
                            return false;
                        }
                        const char escapedCharacter = array.front();
                        array.remove_prefix(1);
                        switch (escapedCharacter) {
                        case '"': value.push_back('"'); break;
                        case '\\': value.push_back('\\'); break;
                        case 'b': value.push_back('\b'); break;
                        case 't': value.push_back('\t'); break;
                        case 'n': value.push_back('\n'); break;
                        case 'f': value.push_back('\f'); break;
                        case 'r': value.push_back('\r'); break;
                        default: return false;
                        }
                    } else {
                        value.push_back(character);
                    }
                }
                if (!closed) {
                    return false;
                }
                parsed.push_back(std::move(value));
                array = TrimView(array);
                if (array.empty()) {
                    return false;
                }
                if (array.front() == ',') {
                    array.remove_prefix(1);
                    continue;
                }
                if (array.front() != ']') {
                    return false;
                }
            }
        }
        if (lineEnd == std::string_view::npos) {
            break;
        }
        lineStart = lineEnd + 1;
    }
    return true;
}

auto LoadPluginConfig(const D2RL::PluginContext* context) noexcept -> void {
    AllowWithoutCube = false;
    IncludedRecipeNames.clear();
    if (context == nullptr || !context->EnsureConfig(DefaultConfig)) {
        if (context != nullptr) {
            context->LogWarn("Could not create or access the plugin TOML config; Cube staging remains required.");
        }
        return;
    }

    try {
        std::array<char, MaximumConfigBytes> buffer{};
        std::uint32_t requiredSize{};
        if (!context->ReadConfig(buffer.data(), static_cast<std::uint32_t>(buffer.size()), &requiredSize)
            || requiredSize > buffer.size()) {
            context->LogWarn("Could not read the plugin TOML config; Cube staging remains required.");
            return;
        }
        const std::string_view config(buffer.data());
        bool configuredValue = false;
        if (ReadTomlBoolean(config, "allow_without_cube", configuredValue)) {
            AllowWithoutCube = configuredValue;
        } else {
            context->LogWarn("The plugin TOML config has no valid allow_without_cube boolean; Cube staging remains required.");
        }
        std::vector<std::string> includedRecipes;
        bool includedRecipesFound{};
        if (ReadTomlStringArray(config, "included-recipes", includedRecipes, includedRecipesFound)) {
            for (auto& recipeName : includedRecipes) {
                recipeName = Lower(Trim(recipeName));
                if (!recipeName.empty()) {
                    IncludedRecipeNames.insert(std::move(recipeName));
                }
            }
        } else if (includedRecipesFound) {
            context->LogWarn("The plugin TOML included-recipes value is invalid; custom recipes are disabled.");
        }
    } catch (...) {
        context->LogWarn("Could not parse the plugin TOML config; Cube staging remains required.");
    }
}

auto SplitTabs(const std::string& line) -> std::vector<std::string> {
    std::vector<std::string> fields;
    std::size_t start = 0;
    while (true) {
        const std::size_t end = line.find('\t', start);
        fields.push_back(Trim(std::string_view(line).substr(start, end == std::string::npos ? end : end - start)));
        if (end == std::string::npos) {
            break;
        }
        start = end + 1;
    }
    if (!fields.empty() && fields.front().size() >= 3
        && static_cast<unsigned char>(fields.front()[0]) == 0xEF
        && static_cast<unsigned char>(fields.front()[1]) == 0xBB
        && static_cast<unsigned char>(fields.front()[2]) == 0xBF) {
        fields.front().erase(0, 3);
    }
    return fields;
}

auto ReadTable(const std::filesystem::path& path, Table& table) -> bool {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        return false;
    }

    std::string line;
    if (!std::getline(input, line)) {
        return false;
    }
    table.headers = SplitTabs(line);
    table.rows.clear();
    while (std::getline(input, line)) {
        auto row = SplitTabs(line);
        if (row.size() < table.headers.size()) {
            row.resize(table.headers.size());
        }
        table.rows.push_back(std::move(row));
    }
    return !table.headers.empty();
}

auto ColumnIndex(const Table& table, std::string_view name) -> std::size_t {
    const std::string wanted = Lower(Trim(name));
    for (std::size_t index = 0; index < table.headers.size(); ++index) {
        if (Lower(table.headers[index]) == wanted) {
            return index;
        }
    }
    return std::numeric_limits<std::size_t>::max();
}

auto Cell(const Table& table, const std::vector<std::string>& row, std::string_view name) -> std::string_view {
    const std::size_t column = ColumnIndex(table, name);
    return column < row.size() ? std::string_view(row[column]) : std::string_view{};
}

auto ParseUnsigned(std::string_view value, std::uint32_t& result) -> bool {
    const std::string trimmed = Trim(value);
    if (trimmed.empty()) {
        return false;
    }
    const char* begin = trimmed.data();
    const char* end = begin + trimmed.size();
    const auto parsed = std::from_chars(begin, end, result);
    return parsed.ec == std::errc{} && parsed.ptr == end;
}

auto ItemCode(std::string_view value, std::uint32_t& code) -> bool {
    const std::string normalized = Lower(Trim(value));
    if (normalized.empty() || normalized.size() > 4) {
        return false;
    }
    std::array<char, 4> characters{ ' ', ' ', ' ', ' ' };
    std::copy(normalized.begin(), normalized.end(), characters.begin());
    code = D2RL::Items::MakeItemCode(characters[0], characters[1], characters[2], characters[3]);
    return true;
}

auto JsonObjectEnd(std::string_view json, std::size_t objectStart) -> std::size_t {
    if (objectStart >= json.size() || json[objectStart] != '{') {
        return std::string_view::npos;
    }
    std::size_t depth = 0;
    bool inString = false;
    bool escaped = false;
    for (std::size_t index = objectStart; index < json.size(); ++index) {
        const char character = json[index];
        if (inString) {
            if (escaped) {
                escaped = false;
            } else if (character == '\\') {
                escaped = true;
            } else if (character == '"') {
                inString = false;
            }
            continue;
        }
        if (character == '"') {
            inString = true;
        } else if (character == '{') {
            ++depth;
        } else if (character == '}' && --depth == 0) {
            return index;
        }
    }
    return std::string_view::npos;
}

auto JsonStringField(std::string_view object, std::string_view key, std::string& value) -> bool {
    std::string quotedKey;
    quotedKey.reserve(key.size() + 2);
    quotedKey.push_back('"');
    quotedKey.append(key);
    quotedKey.push_back('"');
    const std::size_t keyPosition = object.find(quotedKey);
    if (keyPosition == std::string_view::npos) {
        return false;
    }
    const std::size_t colon = object.find(':', keyPosition + quotedKey.size());
    if (colon == std::string_view::npos) {
        return false;
    }
    std::size_t valuePosition = colon + 1;
    while (valuePosition < object.size() && std::isspace(static_cast<unsigned char>(object[valuePosition]))) {
        ++valuePosition;
    }
    if (valuePosition >= object.size() || object[valuePosition] != '"') {
        return false;
    }
    const std::size_t valueStart = ++valuePosition;
    const std::size_t valueEnd = object.find('"', valueStart);
    if (valueEnd == std::string_view::npos) {
        return false;
    }
    value.assign(object.substr(valueStart, valueEnd - valueStart));
    return true;
}

auto JsonIntegerField(std::string_view object, std::string_view key, std::int32_t& value) -> bool {
    std::string quotedKey;
    quotedKey.reserve(key.size() + 2);
    quotedKey.push_back('"');
    quotedKey.append(key);
    quotedKey.push_back('"');
    const std::size_t keyPosition = object.find(quotedKey);
    if (keyPosition == std::string_view::npos) {
        return false;
    }
    const std::size_t colon = object.find(':', keyPosition + quotedKey.size());
    if (colon == std::string_view::npos) {
        return false;
    }
    std::size_t valueStart = colon + 1;
    while (valueStart < object.size() && std::isspace(static_cast<unsigned char>(object[valueStart]))) {
        ++valueStart;
    }
    std::size_t valueEnd = valueStart;
    while (valueEnd < object.size() && (object[valueEnd] == '-' || std::isdigit(static_cast<unsigned char>(object[valueEnd])))) {
        ++valueEnd;
    }
    if (valueStart == valueEnd) {
        return false;
    }
    const auto parsed = std::from_chars(object.data() + valueStart, object.data() + valueEnd, value);
    return parsed.ec == std::errc{} && parsed.ptr == object.data() + valueEnd;
}

auto LoadCraftingSlotLayoutsFromFile(const std::filesystem::path& layoutPath) -> bool {
    std::ifstream input(layoutPath, std::ios::binary);
    if (!input) {
        return false;
    }
    const std::string json{std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
    constexpr std::string_view SlotType = "\"type\": \"AdvancedStashSlotWidget\"";
    std::size_t position = 0;
    while ((position = json.find(SlotType, position)) != std::string::npos) {
        const std::size_t objectStart = json.rfind('{', position);
        const std::size_t objectEnd = JsonObjectEnd(json, objectStart);
        if (objectStart == std::string::npos || objectEnd == std::string::npos) {
            break;
        }
        const std::string_view slotObject(json.data() + objectStart, objectEnd - objectStart + 1);
        CraftingSlotLayout slot{};
        if (JsonStringField(slotObject, "itemCode", slot.itemCode) && !slot.itemCode.empty()) {
            CraftingSlotLayouts.push_back(std::move(slot));
        }
        position = objectEnd + 1;
    }
    return !CraftingSlotLayouts.empty();
}

auto LoadDefaultCraftingSlotLayouts() -> bool {
    CraftingSlotLayouts.clear();
    CraftingSlotLayouts.reserve(DefaultCraftingSlotLayouts.size());
    for (const auto& slot : DefaultCraftingSlotLayouts) {
        CraftingSlotLayouts.push_back(CraftingSlotLayout{
            std::string(slot.itemCode), slot.x, slot.y, slot.width, slot.height });
    }
    return !CraftingSlotLayouts.empty();
}

auto LoadCraftingSlotLayouts(const D2RL::PluginContext* context,
    const std::filesystem::path& excelDirectory) -> bool {
    CraftingSlotLayouts.clear();
    const auto activeLayout = excelDirectory.empty() ? std::filesystem::path{}
        : excelDirectory.parent_path() / L"ui" / L"layouts" / L"bankexpansionlayouthd.json";
    if (!activeLayout.empty()) {
        std::error_code error;
        if (std::filesystem::is_regular_file(activeLayout, error)) {
            if (LoadCraftingSlotLayoutsFromFile(activeLayout)) {
                return true;
            }
            if (context != nullptr) {
                context->LogWarn("Rune Gem Quick Upgrade could not parse Advanced Stash slot coordinates from the active mod's bankexpansionlayouthd.json; using built-in coordinates.");
            }
        }
    }
    return LoadDefaultCraftingSlotLayouts();
}

auto ExcelDirectoryFromContext(const D2RL::PluginContext* context) -> std::filesystem::path {
    if (context == nullptr) {
        return {};
    }

    std::string activeMod;
    constexpr std::uint32_t ActiveModFieldEnd = static_cast<std::uint32_t>(offsetof(D2RL::PluginContext, activeMod) + sizeof(const char*));
    if (context->contextSize >= ActiveModFieldEnd && context->activeMod != nullptr) {
        activeMod = context->activeMod;
    }

    std::vector<std::filesystem::path> roots;
    auto addRoot = [&roots](const std::filesystem::path& path) {
        if (path.empty()) {
            return;
        }
        const auto normalized = path.lexically_normal();
        if (std::find(roots.begin(), roots.end(), normalized) == roots.end()) {
            roots.push_back(normalized);
        }
    };

    constexpr std::uint32_t ModDirectoryFieldEnd = static_cast<std::uint32_t>(offsetof(D2RL::PluginContext, modDirectory) + sizeof(const wchar_t*));
    if (context->contextSize >= ModDirectoryFieldEnd && context->modDirectory != nullptr) {
        addRoot(std::filesystem::path(context->modDirectory));
    }
    constexpr std::uint32_t ConfigPathFieldEnd = static_cast<std::uint32_t>(offsetof(D2RL::PluginContext, pluginConfigPath) + sizeof(const wchar_t*));
    if (context->contextSize >= ConfigPathFieldEnd && context->pluginConfigPath != nullptr) {
        auto path = std::filesystem::path(context->pluginConfigPath).parent_path();
        for (int level = 0; level < 4 && !path.empty(); ++level) {
            addRoot(path);
            path = path.parent_path();
        }
    }
    constexpr std::uint32_t ScopeRootFieldEnd = static_cast<std::uint32_t>(offsetof(D2RL::PluginContext, scopeRootDirectory) + sizeof(const wchar_t*));
    if (context->contextSize >= ScopeRootFieldEnd && context->scopeRootDirectory != nullptr) {
        addRoot(std::filesystem::path(context->scopeRootDirectory));
    }

    std::vector<std::filesystem::path> candidates;
    auto addExcel = [&candidates](const std::filesystem::path& root) {
        if (root.empty()) {
            return;
        }
        const auto path = (root / L"data" / L"global" / L"excel").lexically_normal();
        if (std::find(candidates.begin(), candidates.end(), path) == candidates.end()) {
            candidates.push_back(path);
        }
    };

    const auto modName = activeMod.empty() ? std::filesystem::path{} : std::filesystem::path(activeMod);
    for (const auto& root : roots) {
        addExcel(root);
        if (!activeMod.empty()) {
            addExcel(root / std::filesystem::path(activeMod + ".mpq"));
            addExcel(root / modName);
            addExcel(root / modName / std::filesystem::path(activeMod + ".mpq"));
        }
    }
    constexpr std::array<std::wstring_view, 5> SupportedTables{
        L"cubemain.txt", L"misc.txt", L"itemtypes.txt", L"armor.txt", L"weapons.txt" };
    for (const auto& candidate : candidates) {
        std::error_code error;
        const auto layoutPath = candidate.parent_path() / L"ui" / L"layouts" / L"bankexpansionlayouthd.json";
        if (std::filesystem::is_regular_file(layoutPath, error)) {
            return candidate;
        }
        for (const auto table : SupportedTables) {
            error.clear();
            if (std::filesystem::is_regular_file(candidate / table, error)) {
                return candidate;
            }
        }
    }
    return {};
}

auto RuntimeBuildNumber(const D2RL::PluginContext* context) noexcept -> std::int32_t {
    if (context == nullptr) {
        return 0;
    }
    constexpr std::uint32_t BuildVersionFieldEnd = static_cast<std::uint32_t>(
        offsetof(D2RL::PluginContext, buildVersion) + sizeof(const char*));
    if (context->contextSize >= BuildVersionFieldEnd && context->buildVersion != nullptr) {
        const std::string_view version(context->buildVersion);
        const std::size_t separator = version.find_last_of('.');
        if (separator != std::string_view::npos) {
            std::uint32_t build{};
            if (ParseUnsigned(version.substr(separator + 1), build)
                && build > 10000 && build <= static_cast<std::uint32_t>(std::numeric_limits<std::int32_t>::max())) {
                return static_cast<std::int32_t>(build);
            }
        }
    }
    constexpr std::uint32_t ModDataBuildFieldEnd = static_cast<std::uint32_t>(
        offsetof(D2RL::PluginContext, modDataVersionBuild) + sizeof(std::uint32_t));
    if (context->contextSize >= ModDataBuildFieldEnd
        && context->modDataVersionBuild > 0
        && context->modDataVersionBuild <= static_cast<std::uint32_t>(std::numeric_limits<std::int32_t>::max())) {
        return static_cast<std::int32_t>(context->modDataVersionBuild);
    }
    return 0;
}

auto CompiledExcelBuild(const std::filesystem::path& excelDirectory, std::int32_t& build) -> bool {
    const auto versionDirectory = excelDirectory.parent_path().parent_path().parent_path();
    std::ifstream manifest(versionDirectory / L"manifest.json", std::ios::binary);
    if (!manifest) {
        return false;
    }
    const std::string json{std::istreambuf_iterator<char>(manifest), std::istreambuf_iterator<char>()};
    return JsonIntegerField(json, "gameBuild", build);
}

auto CompiledExcelDirectoryFromContext(const D2RL::PluginContext* context,
    const std::filesystem::path& activeExcelDirectory) -> std::filesystem::path {
    std::vector<std::filesystem::path> candidates;
    auto addCandidatesFrom = [&candidates](std::filesystem::path ancestor) -> bool {
        for (std::uint32_t depth = 0; depth < 8 && !ancestor.empty(); ++depth) {
            const auto compilerRoot = ancestor / L"d2rloader" / L"data" / L"compiler";
            std::error_code error;
            if (std::filesystem::is_directory(compilerRoot, error)) {
                for (std::filesystem::directory_iterator it(compilerRoot, error), end;
                    !error && it != end; it.increment(error)) {
                    if (!it->is_directory(error)) {
                        continue;
                    }
                    const auto excel = it->path() / L"data" / L"global" / L"excel";
                    std::error_code fileError;
                    if (!std::filesystem::is_regular_file(it->path() / L"manifest.json", fileError)) {
                        continue;
                    }
                    constexpr std::array<std::wstring_view, 5> SupportedTables{
                        L"cubemain.txt", L"misc.txt", L"itemtypes.txt", L"armor.txt", L"weapons.txt" };
                    bool hasSupportedTable = false;
                    for (const auto table : SupportedTables) {
                        fileError.clear();
                        if (std::filesystem::is_regular_file(excel / table, fileError)) {
                            hasSupportedTable = true;
                            break;
                        }
                    }
                    if (!hasSupportedTable) {
                        continue;
                    }
                    const auto normalized = excel.lexically_normal();
                    if (std::find(candidates.begin(), candidates.end(), normalized) == candidates.end()) {
                        candidates.push_back(normalized);
                    }
                }
                if (!candidates.empty()) {
                    return true;
                }
            }
            const auto parent = ancestor.parent_path();
            if (parent == ancestor) {
                break;
            }
            ancestor = parent;
        }
        return false;
    };

    const bool foundFromActiveData = addCandidatesFrom(activeExcelDirectory);
    if (!foundFromActiveData && context != nullptr) {
        constexpr std::uint32_t ModDirectoryFieldEnd = static_cast<std::uint32_t>(
            offsetof(D2RL::PluginContext, modDirectory) + sizeof(const wchar_t*));
        if (context->contextSize >= ModDirectoryFieldEnd && context->modDirectory != nullptr) {
            (void)addCandidatesFrom(std::filesystem::path(context->modDirectory));
        }
        constexpr std::uint32_t ScopeRootFieldEnd = static_cast<std::uint32_t>(
            offsetof(D2RL::PluginContext, scopeRootDirectory) + sizeof(const wchar_t*));
        if (candidates.empty() && context->contextSize >= ScopeRootFieldEnd && context->scopeRootDirectory != nullptr) {
            (void)addCandidatesFrom(std::filesystem::path(context->scopeRootDirectory));
        }
    }
    if (candidates.empty()) {
        return {};
    }

    const std::int32_t runtimeBuild = RuntimeBuildNumber(context);
    if (runtimeBuild > 0) {
        std::filesystem::path matching;
        for (const auto& candidate : candidates) {
            std::int32_t candidateBuild{};
            if (CompiledExcelBuild(candidate, candidateBuild) && candidateBuild == runtimeBuild) {
                if (!matching.empty()) {
                    return {};
                }
                matching = candidate;
            }
        }
        if (!matching.empty()) {
            return matching;
        }
    }
    return candidates.size() == 1 ? candidates.front() : std::filesystem::path{};
}

auto ReadModOrCompiledTable(const std::filesystem::path& activeExcelDirectory,
    const std::filesystem::path& compiledExcelDirectory,
    std::wstring_view filename,
    Table& table,
    bool& usedCompiledFallback) -> bool {
    usedCompiledFallback = false;
    if (!activeExcelDirectory.empty()) {
        std::error_code error;
        const auto activePath = activeExcelDirectory / filename;
        if (std::filesystem::is_regular_file(activePath, error)) {
            return ReadTable(activePath, table);
        }
    }
    if (!compiledExcelDirectory.empty()) {
        const auto compiledPath = compiledExcelDirectory / filename;
        std::error_code error;
        if (std::filesystem::is_regular_file(compiledPath, error)
            && ReadTable(compiledPath, table)) {
            usedCompiledFallback = true;
            return true;
        }
    }
    return false;
}

auto ReadGemUpgradeSoundId(const Table& sounds) -> std::uint16_t {
    const std::size_t soundColumn = ColumnIndex(sounds, "Sound");
    const std::size_t indexColumn = ColumnIndex(sounds, "*Index");
    const std::size_t redirectColumn = ColumnIndex(sounds, "Redirect");
    if (soundColumn == std::numeric_limits<std::size_t>::max()
        || indexColumn == std::numeric_limits<std::size_t>::max()) {
        return 0;
    }
    for (const auto& row : sounds.rows) {
        if (soundColumn >= row.size() || Lower(Trim(row[soundColumn])) != "shrine_gemupgrade"
            || indexColumn >= row.size()) {
            continue;
        }
        const std::string redirect = redirectColumn < row.size()
            ? Lower(Trim(row[redirectColumn])) : std::string{};
        if (!redirect.empty()) {
            for (const auto& redirectedRow : sounds.rows) {
                if (soundColumn >= redirectedRow.size()
                    || Lower(Trim(redirectedRow[soundColumn])) != redirect
                    || indexColumn >= redirectedRow.size()) {
                    continue;
                }
                std::uint32_t redirectedSoundId{};
                if (ParseUnsigned(redirectedRow[indexColumn], redirectedSoundId)
                    && redirectedSoundId > 0
                    && redirectedSoundId <= std::numeric_limits<std::uint16_t>::max()) {
                    return static_cast<std::uint16_t>(redirectedSoundId);
                }
                break;
            }
        }
        std::uint32_t soundId{};
        return ParseUnsigned(row[indexColumn], soundId)
                && soundId > 0 && soundId <= std::numeric_limits<std::uint16_t>::max()
            ? static_cast<std::uint16_t>(soundId) : 0;
    }
    return 0;
}

auto LoadGemUpgradeSound(const D2RL::PluginContext* context,
    const std::filesystem::path& activeExcelDirectory,
    const std::filesystem::path& compiledExcelDirectory) -> void {
    GemUpgradeSoundId = 0;
    Table sounds;
    bool usedCompiledFallback{};
    if (ReadModOrCompiledTable(activeExcelDirectory, compiledExcelDirectory,
            L"sounds.txt", sounds, usedCompiledFallback)) {
        GemUpgradeSoundId = ReadGemUpgradeSoundId(sounds);
    }

    // A mod may ship a partial sounds.txt. If it has no gem-upgrade row,
    // fall back to D2RLoader's merged base table for that sound event.
    if (GemUpgradeSoundId == 0 && !usedCompiledFallback && !compiledExcelDirectory.empty()) {
        Table compiledSounds;
        std::error_code error;
        const auto compiledPath = compiledExcelDirectory / L"sounds.txt";
        if (std::filesystem::is_regular_file(compiledPath, error) && ReadTable(compiledPath, compiledSounds)) {
            GemUpgradeSoundId = ReadGemUpgradeSoundId(compiledSounds);
            usedCompiledFallback = GemUpgradeSoundId != 0;
        }
    }

    if (context == nullptr) {
        return;
    }
    if (GemUpgradeSoundId == 0) {
        context->LogWarn("Gem upgrade sound disabled: sounds.txt has no valid shrine_gemupgrade entry.");
        return;
    }
}

auto IsGemOrRuneType(std::string_view type, const std::unordered_map<std::string, std::array<std::string, 2>>& parents, bool& isRune) -> bool {
    std::string current = Lower(Trim(type));
    std::unordered_set<std::string> visited;
    while (!current.empty() && visited.insert(current).second) {
        if (current == "rune") {
            isRune = true;
            return true;
        }
        if (current == "gem" || current == "gemr" || current == "gemz") {
            isRune = false;
            return true;
        }
        const auto found = parents.find(current);
        if (found == parents.end()) {
            break;
        }
        if (!found->second[0].empty()) {
            current = found->second[0];
        } else if (!found->second[1].empty()) {
            current = found->second[1];
        } else {
            break;
        }
    }
    return false;
}

auto IsSimpleCodeSpec(std::string_view value, bool allowQuantity, Ingredient& ingredient) -> bool {
    value = TrimView(value);
    // CubeMain quotes input specs that contain commas, for example
    // "gcr,qty=3". Strip the surrounding Excel quotes before parsing.
    if (value.size() >= 2 && value.front() == '"' && value.back() == '"') {
        value.remove_prefix(1);
        value.remove_suffix(1);
        value = TrimView(value);
    }
    const std::size_t firstComma = value.find(',');
    const auto codeText = value.substr(0, firstComma);
    if (!ItemCode(codeText, ingredient.code)) {
        return false;
    }
    ingredient.quantity = 1;
    if (firstComma == std::string_view::npos) {
        return true;
    }
    if (!allowQuantity) {
        return false;
    }

    std::size_t start = firstComma + 1;
    while (start <= value.size()) {
        const std::size_t end = value.find(',', start);
        const std::string attribute = Lower(Trim(value.substr(start, end == std::string_view::npos ? end : end - start)));
        const std::size_t equals = attribute.find('=');
        if (equals == std::string::npos || attribute.substr(0, equals) != "qty") {
            return false;
        }
        if (!ParseUnsigned(std::string_view(attribute).substr(equals + 1), ingredient.quantity)
            || ingredient.quantity == 0 || ingredient.quantity > MaxInputQuantity) {
            return false;
        }
        if (end == std::string_view::npos) {
            break;
        }
        start = end + 1;
    }
    return true;
}

auto IsDefaultCondition(const Table& cube, const std::vector<std::string>& row) -> bool {
    const auto allowed = [](std::string_view value, std::initializer_list<std::string_view> values) {
        const std::string normalized = Lower(Trim(value));
        if (normalized.empty()) {
            return true;
        }
        return std::find(values.begin(), values.end(), normalized) != values.end();
    };
    return allowed(Cell(cube, row, "version"), { "0", "100" })
        && allowed(Cell(cube, row, "min diff"), { "0" })
        && allowed(Cell(cube, row, "max diff"), { "99" })
        && allowed(Cell(cube, row, "ladder"), { "0" })
        && allowed(Cell(cube, row, "firstladderseason"), { "0" })
        && allowed(Cell(cube, row, "lastladderseason"), { "0" })
        && allowed(Cell(cube, row, "class"), {})
        && allowed(Cell(cube, row, "op"), {})
        && allowed(Cell(cube, row, "param"), {})
        && allowed(Cell(cube, row, "value"), {});
}

auto CountItemRecords(const Table& table) -> std::int32_t {
    const std::size_t codeColumn = ColumnIndex(table, "code");
    if (codeColumn == std::numeric_limits<std::size_t>::max()) {
        return -1;
    }
    std::int32_t count = 0;
    for (const auto& row : table.rows) {
        if (codeColumn >= row.size() || Trim(row[codeColumn]).empty()) {
            continue;
        }
        if (count == std::numeric_limits<std::int32_t>::max()) {
            return -1;
        }
        ++count;
    }
    return count;
}

auto LoadRecipes(const D2RL::PluginContext* context) -> bool {
    Recipes.clear();
    ItemClassByCode.clear();
    AdvancedStashStackableItemCodes.clear();
    GemItemCodes.clear();
    RuneItemCodes.clear();
    const auto excelDirectory = ExcelDirectoryFromContext(context);
    const auto compiledExcelDirectory = CompiledExcelDirectoryFromContext(context, excelDirectory);
    (void)LoadCraftingSlotLayouts(context, excelDirectory);
    LoadGemUpgradeSound(context, excelDirectory, compiledExcelDirectory);

    Table cube;
    Table misc;
    Table types;
    Table armor;
    Table weapons;
    bool cubeFallback{};
    bool miscFallback{};
    bool typesFallback{};
    bool armorFallback{};
    bool weaponsFallback{};
    if (!ReadModOrCompiledTable(excelDirectory, compiledExcelDirectory, L"cubemain.txt", cube, cubeFallback)
        || !ReadModOrCompiledTable(excelDirectory, compiledExcelDirectory, L"misc.txt", misc, miscFallback)
        || !ReadModOrCompiledTable(excelDirectory, compiledExcelDirectory, L"itemtypes.txt", types, typesFallback)
        || !ReadModOrCompiledTable(excelDirectory, compiledExcelDirectory, L"armor.txt", armor, armorFallback)
        || !ReadModOrCompiledTable(excelDirectory, compiledExcelDirectory, L"weapons.txt", weapons, weaponsFallback)) {
        if (context != nullptr) {
            context->LogWarn("Rune Gem Quick Upgrade could not read the required cubemain.txt, misc.txt, itemtypes.txt, armor.txt, or weapons.txt tables from the mod or D2RLoader compiled cache.");
        }
        return false;
    }

    const std::int32_t armorRecordCount = CountItemRecords(armor);
    const std::int32_t weaponRecordCount = CountItemRecords(weapons);
    const std::int32_t miscRecordCount = CountItemRecords(misc);
    if (armorRecordCount < 0 || weaponRecordCount < 0 || miscRecordCount < 0
        || armorRecordCount > std::numeric_limits<std::int32_t>::max() - weaponRecordCount
        || armorRecordCount + weaponRecordCount
            > std::numeric_limits<std::int32_t>::max() - miscRecordCount) {
        context->LogWarn("Rune Gem Quick Upgrade could not establish native item-class offsets from armor.txt and weapons.txt.");
        return false;
    }

    std::unordered_map<std::string, std::array<std::string, 2>> typeParents;
    const auto typeCodeColumn = ColumnIndex(types, "Code");
    const auto equivOneColumn = ColumnIndex(types, "Equiv1");
    const auto equivTwoColumn = ColumnIndex(types, "Equiv2");
    if (typeCodeColumn == std::numeric_limits<std::size_t>::max()) {
        context->LogWarn("Rune Gem Quick Upgrade could not resolve the ItemTypes Code column.");
        return false;
    }
    for (const auto& row : types.rows) {
        if (typeCodeColumn >= row.size()) {
            continue;
        }
        const std::string code = Lower(Trim(row[typeCodeColumn]));
        if (code.empty()) {
            continue;
        }
        std::array<std::string, 2> parents{};
        if (equivOneColumn < row.size()) parents[0] = Lower(Trim(row[equivOneColumn]));
        if (equivTwoColumn < row.size()) parents[1] = Lower(Trim(row[equivTwoColumn]));
        typeParents.emplace(code, std::move(parents));
    }

    std::unordered_map<std::uint32_t, ItemProgression> progressionByCode;
    std::unordered_map<std::uint32_t, bool> stackableByCode;
    auto addItemTable = [&](const Table& table, std::int32_t firstItemClass) -> std::int32_t {
        std::int32_t itemClass = firstItemClass;
        const std::size_t stackableColumn = ColumnIndex(table, "AdvancedStashStackable");
        for (const auto& row : table.rows) {
            const auto codeText = Cell(table, row, "code");
            if (Trim(codeText).empty()) {
                continue;
            }
            std::uint32_t code{};
            if (ItemCode(codeText, code)) {
                ItemClassByCode.insert_or_assign(code, itemClass);
                const std::string itemType = Lower(Trim(Cell(table, row, "type")));
                const bool hasKnownItemType = !itemType.empty() && typeParents.contains(itemType);
                const bool stackable = hasKnownItemType && stackableColumn < row.size()
                    && Trim(row[stackableColumn]) == "1";
                const auto [found, inserted] = stackableByCode.emplace(code, stackable);
                if (!inserted) {
                    found->second = found->second && stackable;
                }
            }
            ++itemClass;
        }
        return itemClass;
    };
    const std::int32_t nextArmorClass = addItemTable(armor, 0);
    const std::int32_t nextWeaponClass = addItemTable(weapons, armorRecordCount);
    const std::int32_t nextMiscClass = addItemTable(misc, armorRecordCount + weaponRecordCount);
    if (nextArmorClass != armorRecordCount
        || nextWeaponClass != armorRecordCount + weaponRecordCount
        || nextMiscClass != armorRecordCount + weaponRecordCount + miscRecordCount) {
        context->LogWarn("Rune Gem Quick Upgrade could not establish native item-class offsets from the active item tables.");
        return false;
    }
    std::unordered_set<std::uint32_t> stackableItemCodes;
    for (const auto& [code, stackable] : stackableByCode) {
        if (stackable) {
            stackableItemCodes.insert(code);
            AdvancedStashStackableItemCodes.insert(code);
        }
    }

    for (const auto& row : misc.rows) {
        std::uint32_t code{};
        const auto codeText = Cell(misc, row, "code");
        if (Trim(codeText).empty()) {
            continue;
        }
        if (!ItemCode(codeText, code)) {
            continue;
        }
        bool isRune = false;
        if (!IsGemOrRuneType(Cell(misc, row, "type"), typeParents, isRune)) {
            continue;
        }

        std::uint32_t rank{};
        if (isRune) {
            const std::string itemCode = Lower(Trim(Cell(misc, row, "code")));
            if (itemCode.size() != 3 || itemCode.front() != 'r'
                || !ParseUnsigned(std::string_view(itemCode).substr(1), rank) || rank == 0) {
                continue;
            }
            RuneItemCodes.insert(code);
            progressionByCode.insert_or_assign(code, ItemProgression{ UpgradeKind::Rune, rank });
        } else {
            GemItemCodes.insert(code);
            const std::string gemTier = Lower(Trim(Cell(misc, row, "type2")));
            if (!gemTier.starts_with("gem")
                || !ParseUnsigned(std::string_view(gemTier).substr(3), rank)) {
                continue;
            }
            progressionByCode.insert_or_assign(code, ItemProgression{ UpgradeKind::Gem, rank });
        }
    }

    const std::size_t inputQuantityColumn = ColumnIndex(cube, "numinputs");
    const std::size_t enabledColumn = ColumnIndex(cube, "enabled");
    if (inputQuantityColumn == std::numeric_limits<std::size_t>::max()
        || enabledColumn == std::numeric_limits<std::size_t>::max()) {
        context->LogWarn("Rune Gem Quick Upgrade could not resolve required CubeMain columns.");
        return false;
    }

    for (std::size_t rowIndex = 0; rowIndex < cube.rows.size(); ++rowIndex) {
        const auto& row = cube.rows[rowIndex];
        if (enabledColumn >= row.size()) {
            continue;
        }
        const std::string enabled = Lower(Trim(row[enabledColumn]));
        if (enabled == "0" || enabled == "false" || enabled == "no" || !IsDefaultCondition(cube, row)) {
            continue;
        }

        const std::string description = Lower(Trim(Cell(cube, row, "description")));
        const bool includedByConfig = !description.empty() && IncludedRecipeNames.contains(description);

        Recipe recipe{};
        recipe.sourceRow = static_cast<std::uint32_t>(rowIndex);
        bool valid = true;
        for (std::uint32_t slot = 1; slot <= 7; ++slot) {
            const std::string inputColumn = "input " + std::to_string(slot);
            const std::string_view value = Cell(cube, row, inputColumn);
            if (Trim(value).empty()) {
                continue;
            }
            Ingredient ingredient{};
            if (!IsSimpleCodeSpec(value, true, ingredient)
                || (!progressionByCode.contains(ingredient.code) && ingredient.code != RejuvenationPotionCode
                    && (!includedByConfig || !stackableItemCodes.contains(ingredient.code)))) {
                valid = false;
                break;
            }
            auto existing = std::find_if(recipe.inputs.begin(), recipe.inputs.end(), [&](const Ingredient& entry) {
                return entry.code == ingredient.code;
            });
            if (existing == recipe.inputs.end()) {
                recipe.inputs.push_back(ingredient);
            } else {
                existing->quantity += ingredient.quantity;
            }
            recipe.totalInputQuantity += ingredient.quantity;
        }
        if (!valid || recipe.inputs.empty() || recipe.inputs.size() > MaxRecipeInputs
            || recipe.totalInputQuantity == 0 || recipe.totalInputQuantity > MaxInputQuantity) {
            continue;
        }

        std::uint32_t declaredInputs{};
        if (!ParseUnsigned(Cell(cube, row, "numinputs"), declaredInputs)
            || declaredInputs != recipe.totalInputQuantity) {
            continue;
        }

        for (const std::string_view outputColumn : { "output", "output b", "output c" }) {
            const auto value = Cell(cube, row, outputColumn);
            if (Trim(value).empty()) {
                continue;
            }
            Ingredient output{};
            if (!IsSimpleCodeSpec(value, false, output)
                || (!progressionByCode.contains(output.code) && output.code != FullRejuvenationPotionCode
                    && (!includedByConfig || !stackableItemCodes.contains(output.code)))) {
                valid = false;
                break;
            }
            recipe.outputs.push_back(output.code);
        }
        if (!valid || recipe.outputs.empty() || recipe.outputs.size() > MaxRecipeOutputs) {
            continue;
        }

        // A TOML entry opts this enabled CubeMain row into the generic path
        // only when every affected item is a validated, dedicated-slot item.
        if (includedByConfig) {
            const bool everyAffectedItemIsStackable = recipe.outputs.size() == 1
                && std::all_of(recipe.inputs.begin(), recipe.inputs.end(), [&](const Ingredient& ingredient) {
                    return ingredient.quantity <= std::numeric_limits<std::uint8_t>::max()
                        && stackableItemCodes.contains(ingredient.code);
                })
                && stackableItemCodes.contains(recipe.outputs.front());
            if (everyAffectedItemIsStackable) {
                Recipes.push_back(std::move(recipe));
            }
            continue;
        }

        // One-output gem upgrades use exactly three same-code gems. Rune
        // upgrades use three same-code runes, or the active table's rune
        // quantity plus exactly one specified gem.
        if (recipe.outputs.size() != 1) {
            continue;
        }
        if (recipe.inputs.size() == 1
            && recipe.inputs.front().code == RejuvenationPotionCode
            && recipe.inputs.front().quantity == 3
            && recipe.totalInputQuantity == 3
            && recipe.outputs.front() == FullRejuvenationPotionCode) {
            Recipes.push_back(std::move(recipe));
            continue;
        }
        const auto outputProgression = progressionByCode.find(recipe.outputs.front());
        if (outputProgression == progressionByCode.end()) {
            continue;
        }

        const Ingredient* runeInput{};
        const Ingredient* gemInput{};
        std::uint32_t runeInputCount{};
        std::uint32_t gemInputCount{};
        bool inputsRanked = true;
        for (const auto& ingredient : recipe.inputs) {
            const auto progression = progressionByCode.find(ingredient.code);
            if (progression == progressionByCode.end()) {
                inputsRanked = false;
                break;
            }
            if (progression->second.kind == UpgradeKind::Rune) {
                runeInput = &ingredient;
                ++runeInputCount;
            } else {
                gemInput = &ingredient;
                ++gemInputCount;
            }
        }
        bool validShape = inputsRanked;
        if (outputProgression->second.kind == UpgradeKind::Gem) {
            const auto inputProgression = recipe.inputs.size() == 1
                ? progressionByCode.find(recipe.inputs.front().code) : progressionByCode.end();
            validShape = validShape && recipe.inputs.size() == 1 && runeInputCount == 0
                && gemInputCount == 1 && recipe.inputs.front().quantity == 3
                && recipe.totalInputQuantity == 3
                && inputProgression != progressionByCode.end()
                && inputProgression->second.kind == UpgradeKind::Gem
                && outputProgression->second.rank == inputProgression->second.rank + 1;
        } else {
            const auto inputProgression = runeInput != nullptr
                ? progressionByCode.find(runeInput->code) : progressionByCode.end();
            const bool plainRuneUpgrade = recipe.inputs.size() == 1 && runeInputCount == 1
                && gemInputCount == 0 && runeInput != nullptr && runeInput->quantity == 3
                && recipe.totalInputQuantity == 3;
            const bool gemCostRuneUpgrade = recipe.inputs.size() == 2 && runeInputCount == 1
                && gemInputCount == 1 && runeInput != nullptr && gemInput != nullptr
                && (runeInput->quantity == 2 || runeInput->quantity == 3)
                && gemInput->quantity == 1
                && recipe.inputs.front().code == runeInput->code
                && recipe.totalInputQuantity == runeInput->quantity + 1;
            validShape = validShape && (plainRuneUpgrade || gemCostRuneUpgrade)
                && inputProgression != progressionByCode.end()
                && inputProgression->second.kind == UpgradeKind::Rune
                && outputProgression->second.rank == inputProgression->second.rank + 1;
        }
        if (!validShape) {
            continue;
        }

        Recipes.push_back(std::move(recipe));
    }

    std::sort(Recipes.begin(), Recipes.end(), [](const Recipe& lhs, const Recipe& rhs) {
        if (lhs.outputs.size() != rhs.outputs.size()) {
            return lhs.outputs.size() > rhs.outputs.size();
        }
        if (lhs.totalInputQuantity != rhs.totalInputQuantity) {
            return lhs.totalInputQuantity < rhs.totalInputQuantity;
        }
        return lhs.sourceRow < rhs.sourceRow;
    });

    return !Recipes.empty();
}

auto PlayGemUpgradeSound(const D2RL::PluginContext* context, const Recipe& recipe) noexcept -> void {
    if (recipe.outputs.size() != 1 || !GemItemCodes.contains(recipe.outputs.front())) {
        return;
    }
    if (GemUpgradeSoundId == 0 || AttachSound == nullptr || GetLocalDataContext == nullptr
        || GetLocalPlayer == nullptr || GetNativeUnitId == nullptr) {
        if (context != nullptr && !LoggedGemUpgradeSoundUnavailable.exchange(true, std::memory_order_relaxed)) {
            context->LogWarn("Gem upgrade completed, but its sound could not play because the sound entry or native sound function is unavailable.");
        }
        return;
    }

    const std::int32_t dataContext = GetLocalDataContext();
    void* const nativePlayer = dataContext >= 0 ? GetLocalPlayer(dataContext) : nullptr;
    if (nativePlayer == nullptr || GetNativeUnitId(nativePlayer) < 0) {
        if (context != nullptr && !LoggedGemUpgradeSoundUnavailable.exchange(true, std::memory_order_relaxed)) {
            context->LogWarn("Gem upgrade completed, but its sound could not play because the native player was unavailable.");
        }
        return;
    }
    AttachSound(nativePlayer, GemUpgradeSoundId, nativePlayer);
}

struct StashItems {
    std::vector<D2RL::Items::ItemInfo> items;
    bool allocationFailed{};
};

struct ContainerItemsCollection {
    StashItems* items{};
    D2RL::Items::ItemContainer container{D2RL::Items::ItemContainer::Unknown};
};

auto __cdecl CollectContainerItem(const D2RL::PluginContext*, const D2RL::Items::ItemInfo* item,
    void* userData) noexcept -> D2RL::Inventory::IterationAction {
    auto* collection = static_cast<ContainerItemsCollection*>(userData);
    if (collection == nullptr || collection->items == nullptr || item == nullptr
        || item->structSize < D2RL::Items::ItemInfoRequiredSize) {
        return D2RL::Inventory::IterationAction::Stop;
    }
    if (item->container != collection->container) {
        return D2RL::Inventory::IterationAction::Continue;
    }
    try {
        collection->items->items.push_back(*item);
    } catch (...) {
        collection->items->allocationFailed = true;
        return D2RL::Inventory::IterationAction::Stop;
    }
    return D2RL::Inventory::IterationAction::Continue;
}

auto GetLogicalQuantity(const D2RL::Items::ItemInfo& item) noexcept -> std::uint32_t {
    return item.quantity > 0 ? static_cast<std::uint32_t>(item.quantity) : 1U;
}

auto GetStagedQuantity(const std::vector<D2RL::Items::ItemInfo>& items,
    D2RL::Items::ItemContainer container, std::uint32_t code) noexcept -> std::uint64_t {
    std::uint64_t quantity = 0;
    for (const auto& item : items) {
        if (item.container == container && item.code == code) {
            quantity += GetLogicalQuantity(item);
        }
    }
    return quantity;
}

auto GetAddedStagedQuantity(const std::vector<D2RL::Items::ItemInfo>& before,
    const std::vector<D2RL::Items::ItemInfo>& after, D2RL::Items::ItemContainer container,
    std::uint32_t code) noexcept -> std::uint64_t {
    const std::uint64_t beforeQuantity = GetStagedQuantity(before, container, code);
    const std::uint64_t afterQuantity = GetStagedQuantity(after, container, code);
    return afterQuantity > beforeQuantity ? afterQuantity - beforeQuantity : 0;
}

auto BuildStagedTransactionInputs(const std::vector<Ingredient>& ingredients,
    const std::vector<D2RL::Items::ItemInfo>& before,
    const std::vector<D2RL::Items::ItemInfo>& after,
    D2RL::Items::ItemContainer container,
    std::vector<D2RL::Items::TransactionInput>& transactionInputs) -> bool {
    if (ingredients.empty()) {
        return false;
    }

    std::unordered_map<D2RL::ItemHandle, std::uint32_t> baselineByHandle;
    baselineByHandle.reserve(before.size());
    for (const auto& item : before) {
        if (item.container == container
            && std::any_of(ingredients.begin(), ingredients.end(), [&](const Ingredient& ingredient) {
                return ingredient.code == item.code;
            })) {
            baselineByHandle.insert_or_assign(item.handle, GetLogicalQuantity(item));
        }
    }

    std::unordered_map<D2RL::ItemHandle, std::uint32_t> addedByHandle;
    addedByHandle.reserve(after.size());
    for (const auto& item : after) {
        if (item.container != container
            || std::none_of(ingredients.begin(), ingredients.end(), [&](const Ingredient& ingredient) {
                return ingredient.code == item.code;
            })) {
            continue;
        }
        const auto baseline = baselineByHandle.find(item.handle);
        const std::uint32_t priorQuantity = baseline == baselineByHandle.end() ? 0 : baseline->second;
        const std::uint32_t currentQuantity = GetLogicalQuantity(item);
        if (currentQuantity > priorQuantity) {
            addedByHandle.insert_or_assign(item.handle, currentQuantity - priorQuantity);
        }
    }

    std::unordered_map<D2RL::ItemHandle, std::uint32_t> takeByHandle;
    takeByHandle.reserve(after.size());
    for (const auto& ingredient : ingredients) {
        std::uint32_t remaining = ingredient.quantity;
        for (const auto& item : after) {
            if (remaining == 0) {
                break;
            }
            if (item.container != container || item.code != ingredient.code) {
                continue;
            }
            const auto added = addedByHandle.find(item.handle);
            if (added == addedByHandle.end()) {
                continue;
            }
            const std::uint32_t alreadyTaken = takeByHandle.contains(item.handle) ? takeByHandle[item.handle] : 0;
            const std::uint32_t available = added->second > alreadyTaken ? added->second - alreadyTaken : 0;
            const std::uint32_t take = std::min(remaining, available);
            if (take == 0) {
                continue;
            }
            takeByHandle[item.handle] = alreadyTaken + take;
            remaining -= take;
        }
        if (remaining != 0) {
            return false;
        }
    }

    transactionInputs.clear();
    transactionInputs.reserve(after.size());
    for (const auto& item : after) {
        const auto take = takeByHandle.find(item.handle);
        if (take == takeByHandle.end()) {
            continue;
        }
        transactionInputs.push_back({
            .item = item.handle,
            .quantity = take->second,
            .socketedItemPolicy = D2RL::Items::SocketedItemPolicy::RejectIfNotEmpty,
        });
    }
    return !transactionInputs.empty();
}

void __cdecl BeginNativeSlotUpgradeOnGameThread(const D2RL::PluginContext* context, void*) noexcept;
void __cdecl PollNativeSlotUpgradeOnGameThread(const D2RL::PluginContext* context, void*) noexcept;

auto FindRecipeByRow(std::uint32_t sourceRow) noexcept -> const Recipe* {
    const auto found = std::find_if(Recipes.begin(), Recipes.end(), [sourceRow](const Recipe& recipe) {
        return recipe.sourceRow == sourceRow;
    });
    return found == Recipes.end() ? nullptr : &*found;
}

auto PublishTemporarySpriteCodes(const Recipe& recipe) noexcept -> void {
    TemporarySpriteCodeCount.store(0, std::memory_order_release);
    std::uint32_t count{};
    const auto append = [&](std::uint32_t code) noexcept {
        if (code == 0 || count >= TemporarySpriteCodes.size()) {
            return;
        }
        for (std::uint32_t index = 0; index < count; ++index) {
            if (TemporarySpriteCodes[index].load(std::memory_order_relaxed) == code) {
                return;
            }
        }
        TemporarySpriteCodes[count++].store(code, std::memory_order_relaxed);
    };
    for (const auto& ingredient : recipe.inputs) {
        append(ingredient.code);
    }
    for (const std::uint32_t output : recipe.outputs) {
        append(output);
    }
    TemporarySpriteCodeCount.store(count, std::memory_order_release);
}

auto ClearTemporarySpriteCodes() noexcept -> void {
    TemporarySpriteCodeCount.store(0, std::memory_order_release);
}

auto IsTemporarySpriteCode(std::uint32_t code) noexcept -> bool {
    const std::uint32_t count = TemporarySpriteCodeCount.load(std::memory_order_acquire);
    if (count == 0 || count > TemporarySpriteCodes.size()) {
        return false;
    }
    for (std::uint32_t index = 0; index < count; ++index) {
        if (TemporarySpriteCodes[index].load(std::memory_order_relaxed) == code) {
            return true;
        }
    }
    return false;
}

auto ShouldHideTemporarySprite(void* widget, void* item) noexcept -> bool {
    if (widget == nullptr || item == nullptr || MainImageBase == 0 || GetNativeItemCode == nullptr
        || TemporarySpriteCodeCount.load(std::memory_order_acquire) == 0) {
        return false;
    }

    std::uintptr_t widgetVtable{};
    SIZE_T bytesRead{};
    if (!ReadProcessMemory(GetCurrentProcess(), widget, &widgetVtable, sizeof(widgetVtable), &bytesRead)
        || bytesRead != sizeof(widgetVtable)
        || widgetVtable == MainImageBase + AdvancedStashSlotVtableRva) {
        return false;
    }

    const std::uint32_t itemCode = GetNativeItemCode(item);
    if (!IsTemporarySpriteCode(itemCode)) {
        return false;
    }
    return true;
}

auto __fastcall DrawInventoryItemWithTemporarySpritesHidden(void* widget, void* item) noexcept -> void {
    const InventoryItemRenderer original = OriginalInventoryItemRenderer;
    if (original == nullptr) {
        return;
    }
    if (ShouldHideTemporarySprite(widget, item)) {
        return;
    }
    original(widget, item);
}

auto __fastcall DrawInventoryItemWidgetWithTemporarySlotsHidden(void* widget) noexcept -> void {
    const InventoryItemWidgetDraw original = OriginalInventoryItemWidgetDraw;
    if (original == nullptr) {
        return;
    }
    if (widget != nullptr && MainImageBase != 0
        && HideTemporaryInventoryWidgets.load(std::memory_order_acquire)) {
        std::uintptr_t widgetVtable{};
        SIZE_T bytesRead{};
        if (ReadProcessMemory(GetCurrentProcess(), widget, &widgetVtable,
                sizeof(widgetVtable), &bytesRead)
            && bytesRead == sizeof(widgetVtable)
            && widgetVtable == MainImageBase + InventoryItemWidgetVtableRva) {
            return;
        }
    }
    original(widget);
}

auto __fastcall DrawInventorySlotWidgetWithTemporarySlotsHidden(void* widget) noexcept -> void {
    const InventorySlotWidgetDraw original = OriginalInventorySlotWidgetDraw;
    if (original == nullptr) {
        return;
    }
    if (widget != nullptr && MainImageBase != 0) {
        std::uintptr_t widgetVtable{};
        SIZE_T bytesRead{};
        if (ReadProcessMemory(GetCurrentProcess(), widget, &widgetVtable, sizeof(widgetVtable), &bytesRead)
            && bytesRead == sizeof(widgetVtable)) {
            if (widgetVtable == MainImageBase + InventorySlotWidgetVtableRva
                && HideTemporaryInventoryWidgets.load(std::memory_order_acquire)) {
                return;
            }
        }
    }
    original(widget);
}

auto ReadStagingItems(const D2RL::PluginContext* context, D2RL::PlayerHandle player,
    D2RL::Items::ItemContainer container, StashItems& items) noexcept -> D2RL::Inventory::Result {
    items.items.clear();
    items.allocationFailed = false;
    const D2RL::Inventory::ItemFilter filter {
        .structSize = D2RL::Inventory::ItemFilterSize,
        .containerMask = D2RL::Items::ContainerBit(container),
    };
    ContainerItemsCollection collection{ .items = &items, .container = container };
    return InventoryService->forEachInventoryItem(context, player, &filter, CollectContainerItem, &collection);
}

struct CubeSearchState {
    bool found{};
};

auto __cdecl FindHoradricCubeInInventory(const D2RL::PluginContext*, const D2RL::Items::ItemInfo* item,
    void* userData) noexcept -> D2RL::Inventory::IterationAction {
    auto* state = static_cast<CubeSearchState*>(userData);
    if (state == nullptr || item == nullptr || item->structSize < D2RL::Items::ItemInfoRequiredSize) {
        return D2RL::Inventory::IterationAction::Stop;
    }
    if (item->container == D2RL::Items::ItemContainer::Inventory && item->code == HoradricCubeCode) {
        state->found = true;
        return D2RL::Inventory::IterationAction::Stop;
    }
    return D2RL::Inventory::IterationAction::Continue;
}

auto HasHoradricCubeInInventory(const D2RL::PluginContext* context, D2RL::PlayerHandle player,
    bool& found) noexcept -> D2RL::Inventory::Result {
    found = false;
    const D2RL::Inventory::ItemFilter filter {
        .structSize = D2RL::Inventory::ItemFilterSize,
        .containerMask = D2RL::Items::ContainerBit(D2RL::Items::ItemContainer::Inventory),
    };
    CubeSearchState state{};
    const auto result = InventoryService->forEachInventoryItem(context, player, &filter,
        FindHoradricCubeInInventory, &state);
    found = state.found;
    return result;
}

auto FinishNativeSlotUpgrade() noexcept -> void {
    HideTemporaryInventoryWidgets.store(false, std::memory_order_release);
    ClearTemporarySpriteCodes();
    PendingSlotRequest = PendingSlotUpgrade{};
    RequestInFlight.store(false, std::memory_order_release);
}

auto QueueNativeSlotOperation(const D2RL::PluginContext* context, void* widget, std::uint32_t clickedCode,
    const Recipe& recipe, SlotAction action) noexcept -> bool {
    if (recipe.inputs.empty() || recipe.outputs.size() != 1
        || (action != SlotAction::Upgrade && action != SlotAction::Downgrade)) {
        return false;
    }
    bool expected = false;
    if (!RequestInFlight.compare_exchange_strong(expected, true, std::memory_order_acq_rel)) {
        return true;
    }
    try {
        PendingSlotRequest = PendingSlotUpgrade{};
        PendingSlotRequest.widget = widget;
        PendingSlotRequest.player = D2RL::InvalidPlayerHandle;
        PendingSlotRequest.clickedCode = clickedCode;
        PendingSlotRequest.sourceRow = recipe.sourceRow;
        PendingSlotRequest.action = action;
        PendingSlotRequest.stagingContainer = D2RL::Items::ItemContainer::Cube;
        if (action == SlotAction::Downgrade) {
            PendingSlotRequest.transactionIngredients.push_back({ recipe.outputs.front(), 1 });
            PendingSlotRequest.createdItems = recipe.inputs;
        } else {
            PendingSlotRequest.transactionIngredients = recipe.inputs;
            PendingSlotRequest.createdItems.reserve(recipe.outputs.size());
            for (const std::uint32_t code : recipe.outputs) {
                PendingSlotRequest.createdItems.push_back({ code, 1 });
            }
        }
        PublishTemporarySpriteCodes(recipe);
    } catch (...) {
        context->LogError("Could not prepare the Crafting-slot transaction; no stash items were withdrawn.");
        FinishNativeSlotUpgrade();
        return false;
    }
    const auto result = ThreadService->runOnGameThread(context, BeginNativeSlotUpgradeOnGameThread, nullptr);
    if (result != D2RL::Threads::Result::Success) {
        context->LogWarn("Could not queue the Crafting-slot operation on the authoritative game thread; no stash item was withdrawn.");
        FinishNativeSlotUpgrade();
        return false;
    }
    return true;
}

struct NativeOutputTransferState {
    std::uint32_t expectedCode{};
    bool callbackRan{};
    bool transferAttempted{};
    bool transferred{};
};

auto MarkAdvancedStashItemIdentified(void* nativeItem, std::uint32_t expectedCode) noexcept -> bool {
    // A single-item stash slot may have a zero counter byte; the native item
    // identity and type are the occupancy checks for this operation.
    if (nativeItem == nullptr || expectedCode == 0 || GetNativeUnitId == nullptr
        || GetNativeItemCode == nullptr
        || GetNativeUnitId(nativeItem) < 0 || GetNativeItemCode(nativeItem) != expectedCode) {
        return false;
    }

    std::array<std::uint32_t, 4> unitFields{};
    SIZE_T bytesRead{};
    if (!ReadProcessMemory(GetCurrentProcess(), nativeItem, unitFields.data(), sizeof(unitFields), &bytesRead)
        || bytesRead != sizeof(unitFields) || unitFields[0] != 4) {
        return false;
    }

    const std::uintptr_t nativeItemAddress = reinterpret_cast<std::uintptr_t>(nativeItem);
    if (nativeItemAddress > std::numeric_limits<std::uintptr_t>::max() - NativeItemDataPointerOffset) {
        return false;
    }
    std::uintptr_t itemDataAddress{};
    bytesRead = 0;
    const auto* const itemDataPointerAddress = reinterpret_cast<const void*>(nativeItemAddress + NativeItemDataPointerOffset);
    if (!ReadProcessMemory(GetCurrentProcess(), itemDataPointerAddress, &itemDataAddress,
            sizeof(itemDataAddress), &bytesRead)
        || bytesRead != sizeof(itemDataAddress)
        || itemDataAddress % alignof(std::uint32_t) != 0
        || itemDataAddress > std::numeric_limits<std::uintptr_t>::max() - NativeItemFlagsOffset - sizeof(std::uint32_t)) {
        return false;
    }

    std::uint32_t quality{};
    bytesRead = 0;
    const auto* const itemQualityAddress = reinterpret_cast<const void*>(itemDataAddress + NativeItemQualityOffset);
    if (!ReadProcessMemory(GetCurrentProcess(), itemQualityAddress, &quality, sizeof(quality), &bytesRead)
        || bytesRead != sizeof(quality)
        || quality != static_cast<std::uint32_t>(D2RL::Items::Quality::Normal)) {
        return false;
    }

    const std::uintptr_t flagsAddress = itemDataAddress + NativeItemFlagsOffset;
    MEMORY_BASIC_INFORMATION memoryInfo{};
    if (VirtualQuery(reinterpret_cast<const void*>(flagsAddress), &memoryInfo, sizeof(memoryInfo)) != sizeof(memoryInfo)
        || memoryInfo.State != MEM_COMMIT || (memoryInfo.Protect & (PAGE_GUARD | PAGE_NOACCESS)) != 0) {
        return false;
    }
    const DWORD pageProtection = memoryInfo.Protect & 0xFFU;
    if (pageProtection != PAGE_READWRITE && pageProtection != PAGE_WRITECOPY
        && pageProtection != PAGE_EXECUTE_READWRITE && pageProtection != PAGE_EXECUTE_WRITECOPY) {
        return false;
    }
    const std::uintptr_t regionStart = reinterpret_cast<std::uintptr_t>(memoryInfo.BaseAddress);
    if (regionStart > flagsAddress || memoryInfo.RegionSize < sizeof(std::uint32_t)
        || flagsAddress - regionStart > memoryInfo.RegionSize - sizeof(std::uint32_t)) {
        return false;
    }

    std::uint32_t flags{};
    bytesRead = 0;
    if (!ReadProcessMemory(GetCurrentProcess(), reinterpret_cast<const void*>(flagsAddress),
            &flags, sizeof(flags), &bytesRead)
        || bytesRead != sizeof(flags)) {
        return false;
    }
    if ((flags & NativeItemIdentifiedMask) != 0) {
        return true;
    }

    const std::uint32_t identifiedFlags = flags | NativeItemIdentifiedMask;
    SIZE_T bytesWritten{};
    if (!WriteProcessMemory(GetCurrentProcess(), reinterpret_cast<void*>(flagsAddress),
            &identifiedFlags, sizeof(identifiedFlags), &bytesWritten)
        || bytesWritten != sizeof(identifiedFlags)) {
        return false;
    }

    std::uint32_t verifiedFlags{};
    bytesRead = 0;
    return ReadProcessMemory(GetCurrentProcess(), reinterpret_cast<const void*>(flagsAddress),
            &verifiedFlags, sizeof(verifiedFlags), &bytesRead)
        && bytesRead == sizeof(verifiedFlags)
        && (verifiedFlags & NativeItemIdentifiedMask) != 0
        && (verifiedFlags & ~NativeItemIdentifiedMask) == (identifiedFlags & ~NativeItemIdentifiedMask);
}

void __cdecl TransferNativeOutputToAdvancedStash(const D2RL::PluginContext* context, void* nativeItem, void* userData) noexcept {
    auto* state = static_cast<NativeOutputTransferState*>(userData);
    if (state == nullptr) {
        return;
    }
    state->callbackRan = true;
    if (nativeItem == nullptr || GetNativeItemCode == nullptr || GetNativeUnitId == nullptr
        || GetLocalDataContext == nullptr || GetLocalPlayer == nullptr
        || GetAdvancedStashDestination == nullptr || TransferItemToInventoryPage == nullptr
        || FinishInventoryInteraction == nullptr
        || GetNativeItemCode(nativeItem) != state->expectedCode
        || GetNativeUnitId(nativeItem) < 0) {
        return;
    }

    const std::int32_t dataContext = GetLocalDataContext();
    void* const nativePlayer = dataContext >= 0 ? GetLocalPlayer(dataContext) : nullptr;
    void* const destination = nativePlayer != nullptr ? GetAdvancedStashDestination(nativePlayer) : nullptr;
    if (destination == nullptr || GetNativeUnitId(destination) < 0) {
        return;
    }

    std::array<std::uint8_t, 16> placement{};
    state->transferAttempted = true;
    state->transferred = TransferItemToInventoryPage(nativeItem, destination, AdvancedStashPage,
        MainInventoryPage, true, placement.data());
    if (state->transferred) {
        const auto itemClass = ItemClassByCode.find(state->expectedCode);
        void* const stashItem = itemClass != ItemClassByCode.end() && FindAdvancedStashSlotByClass != nullptr
            ? FindAdvancedStashSlotByClass(destination, itemClass->second) : nullptr;
        if (!MarkAdvancedStashItemIdentified(stashItem, state->expectedCode)) {
            if (context != nullptr) {
                context->LogWarn("The output was deposited, but the Advanced Stash slot's identified flag could not be updated.");
            }
        }
    }
    FinishInventoryInteraction(3, nullptr, 0, 0, false);
}

auto CreateRecipeItemsAndDeposit(const D2RL::PluginContext* context, const Recipe& recipe,
    const std::vector<D2RL::Items::TransactionInput>& inputs,
    const std::vector<Ingredient>& createdItems) noexcept -> bool {
    if (inputs.empty() || createdItems.empty() || ItemService == nullptr
        || ItemService->executeTransaction == nullptr || ItemService->editItem == nullptr
        || ItemService->editNativeItem == nullptr) {
        context->LogWarn("The item transaction or native transfer service is unavailable; withdrawn ingredients remain in their staging container.");
        return false;
    }

    try {
        std::vector<Ingredient> individualItems;
        for (const auto& createdItem : createdItems) {
            if (createdItem.code == 0 || createdItem.quantity == 0
                || createdItem.quantity > std::numeric_limits<std::uint8_t>::max()
                || !ItemClassByCode.contains(createdItem.code)
                || individualItems.size() + createdItem.quantity > D2RL::Items::MaxTransactionOutputs) {
                context->LogWarn("A recipe output could not be expanded into validated Inventory items; staged ingredients remain available.");
                return false;
            }
            for (std::uint32_t count = 0; count < createdItem.quantity; ++count) {
                individualItems.push_back({ createdItem.code, 1 });
            }
        }

        std::vector<D2RL::Items::ItemCreateSpec> outputs;
        outputs.reserve(individualItems.size());
        for (const auto& createdItem : individualItems) {
            outputs.push_back({
                .structSize = D2RL::Items::ItemCreateSpecSize,
                .code = createdItem.code,
                .quality = D2RL::Items::Quality::Normal,
                .qualityRecordId = D2RL::Items::RandomQualityRecord,
                .itemLevel = 1,
                .seedMode = D2RL::Items::SeedMode::Random,
                .quantity = D2RL::Items::DefaultValue,
                .durability = D2RL::Items::DefaultValue,
                .stateFlags = D2RL::Items::ItemStateIdentified,
                .destination = {
                    .structSize = D2RL::Items::ItemDestinationSize,
                    .container = D2RL::Items::ItemContainer::Inventory,
                    .placement = D2RL::Items::Placement::Automatic,
                    .x = 0,
                    .y = 0,
                },
            });
        }

        std::vector<D2RL::ItemHandle> outputHandles(individualItems.size(), D2RL::InvalidItemHandle);
        const D2RL::Items::Transaction transaction {
            .structSize = D2RL::Items::TransactionSize,
            .player = PendingSlotRequest.player,
            .inputCount = static_cast<std::uint32_t>(inputs.size()),
            .outputCount = static_cast<std::uint32_t>(outputs.size()),
            .inputs = inputs.data(),
            .outputs = outputs.data(),
            .outputItems = outputHandles.data(),
            .outputCapacity = static_cast<std::uint32_t>(outputHandles.size()),
        };
        D2RL::Items::TransactionResult result {
            .structSize = D2RL::Items::TransactionResultSize,
        };
        const auto transactionResult = ItemService->executeTransaction(context, &transaction, &result);
        if (transactionResult != D2RL::Items::Result::Success) {
            char message[192]{};
            std::snprintf(message, sizeof(message),
                "Recipe row %u transaction failed with result %u; the staged ingredients were not consumed.",
                recipe.sourceRow, static_cast<std::uint32_t>(transactionResult));
            context->LogWarn(message);
            return false;
        }
        if (result.outputCount != static_cast<std::uint32_t>(individualItems.size())
            || std::any_of(outputHandles.begin(), outputHandles.end(), [](D2RL::ItemHandle handle) {
                return handle == D2RL::InvalidItemHandle;
            })) {
            context->LogWarn("The recipe transaction committed, but its output handles could not be verified; check Inventory and the affected material slots.");
            return false;
        }

        for (std::size_t index = 0; index < individualItems.size(); ++index) {
            const auto& createdItem = individualItems[index];
            const D2RL::ItemHandle outputHandle = outputHandles[index];
            D2RL::Items::ItemInfo outputInfo {
                .structSize = D2RL::Items::ItemInfoSize,
            };
            if (ItemService->getItemInfo(context, outputHandle, &outputInfo) != D2RL::Items::Result::Success
                || outputInfo.code != createdItem.code
                || GetLogicalQuantity(outputInfo) != 1
                || outputInfo.container != D2RL::Items::ItemContainer::Inventory
                || outputInfo.inventoryPage != MainInventoryPage) {
                context->LogWarn("Recipe transaction created an item whose inventory identity could not be verified; check Inventory for recovery.");
                return false;
            }

            const D2RL::Items::ItemEdit identifyEdit {
                .structSize = D2RL::Items::ItemEditSize,
                .fields = D2RL::Items::EditFieldBit(D2RL::Items::EditField::Identified),
                .stateFlags = D2RL::Items::ItemStateIdentified,
            };
            if (ItemService->editItem(context, PendingSlotRequest.player, outputHandle, &identifyEdit)
                    != D2RL::Items::Result::Success
                || ItemService->getItemInfo(context, outputHandle, &outputInfo) != D2RL::Items::Result::Success
                || outputInfo.code != createdItem.code
                || GetLogicalQuantity(outputInfo) != 1
                || (outputInfo.stateFlags & D2RL::Items::ItemStateIdentified) == 0) {
                context->LogWarn("A recipe output could not be marked identified; check Inventory for recovery.");
                return false;
            }

            NativeOutputTransferState transferState{ .expectedCode = createdItem.code };
            const auto editResult = ItemService->editNativeItem(context, outputHandle,
                TransferNativeOutputToAdvancedStash, &transferState);
            if (editResult != D2RL::Items::Result::Success || !transferState.callbackRan
                || !transferState.transferAttempted || !transferState.transferred) {
                char message[224]{};
                std::snprintf(message, sizeof(message),
                    "Recipe row %u created output %08X, but not all outputs could be deposited; check Inventory for remaining items.",
                    recipe.sourceRow, createdItem.code);
                context->LogWarn(message);
                return false;
            }
        }
        return true;
    } catch (...) {
        context->LogError("Could not finish creating and depositing the recipe outputs; check Inventory for recovery.");
        return false;
    }
}

void __cdecl PollNativeSlotUpgradeOnGameThread(const D2RL::PluginContext* context, void*) noexcept {
    if (context == nullptr || !RequestInFlight.load(std::memory_order_acquire)
        || !PendingSlotRequest.withdrawalSent) {
        return;
    }
    try {
        const auto stagingContainer = PendingSlotRequest.stagingContainer;
        const char* const stagingName = stagingContainer == D2RL::Items::ItemContainer::Inventory
            ? "Inventory" : "Cube";
        StashItems stagedItems{};
        const auto result = ReadStagingItems(context, PendingSlotRequest.player, stagingContainer, stagedItems);
        if (result != D2RL::Inventory::Result::Success || stagedItems.allocationFailed) {
            char message[192]{};
            std::snprintf(message, sizeof(message),
                "Could not inspect %s after the Advanced Stash withdrawal; ingredients, if received, remain there.",
                stagingName);
            context->LogWarn(message);
            FinishNativeSlotUpgrade();
            return;
        }

        const Recipe* recipe = FindRecipeByRow(PendingSlotRequest.sourceRow);
        if (recipe == nullptr) {
            char message[160]{};
            std::snprintf(message, sizeof(message),
                "The active recipe disappeared while waiting for ingredients; they remain in %s.", stagingName);
            context->LogWarn(message);
            FinishNativeSlotUpgrade();
            return;
        }
        const bool ingredientsArrived = std::all_of(PendingSlotRequest.transactionIngredients.begin(),
            PendingSlotRequest.transactionIngredients.end(), [&](const Ingredient& ingredient) {
            return GetAddedStagedQuantity(PendingSlotRequest.stagedItemsBefore, stagedItems.items,
                stagingContainer, ingredient.code) >= ingredient.quantity;
        });
        if (ingredientsArrived) {
            std::vector<D2RL::Items::TransactionInput> inputs;
            if (!BuildStagedTransactionInputs(PendingSlotRequest.transactionIngredients,
                    PendingSlotRequest.stagedItemsBefore,
                    stagedItems.items, stagingContainer, inputs)) {
                char message[200]{};
                std::snprintf(message, sizeof(message),
                    "Withdrawn recipe ingredients appeared in %s, but their item handles could not be assembled; they remain there.",
                    stagingName);
                context->LogWarn(message);
                FinishNativeSlotUpgrade();
                return;
            }
            if (CreateRecipeItemsAndDeposit(context, *recipe, inputs, PendingSlotRequest.createdItems)
                && PendingSlotRequest.action == SlotAction::Upgrade) {
                PlayGemUpgradeSound(context, *recipe);
            }
            FinishNativeSlotUpgrade();
            return;
        }

        if (++PendingSlotRequest.pollCount >= MaximumWithdrawalPolls) {
            char message[160]{};
            std::snprintf(message, sizeof(message),
                "Timed out waiting for all withdrawn recipe ingredients in %s; check there before retrying.", stagingName);
            context->LogWarn(message);
            FinishNativeSlotUpgrade();
            return;
        }
        if (ThreadService->runOnGameThread(context, PollNativeSlotUpgradeOnGameThread, nullptr)
            != D2RL::Threads::Result::Success) {
            char message[144]{};
            std::snprintf(message, sizeof(message), "Could not continue waiting for the withdrawal; check %s before retrying.", stagingName);
            context->LogWarn(message);
            FinishNativeSlotUpgrade();
        }
    } catch (...) {
        context->LogError("Rune Gem Quick Upgrade stopped safely while waiting for the Advanced Stash withdrawal.");
        FinishNativeSlotUpgrade();
    }
}

void __cdecl BeginNativeSlotUpgradeOnGameThread(const D2RL::PluginContext* context, void*) noexcept {
    if (context == nullptr || !RequestInFlight.load(std::memory_order_acquire)) {
        FinishNativeSlotUpgrade();
        return;
    }
    try {
        const Recipe* recipe = FindRecipeByRow(PendingSlotRequest.sourceRow);
        const bool recipeMatchesAction = recipe != nullptr && !recipe->inputs.empty()
            && recipe->outputs.size() == 1
            && ((PendingSlotRequest.action == SlotAction::Upgrade
                    && recipe->inputs.front().code == PendingSlotRequest.clickedCode)
                || (PendingSlotRequest.action == SlotAction::Downgrade
                    && recipe->outputs.front() == PendingSlotRequest.clickedCode));
        if (!recipeMatchesAction || PendingSlotRequest.transactionIngredients.empty()
            || PendingSlotRequest.createdItems.empty()) {
            context->LogWarn("The queued Crafting-slot recipe no longer matches the active one-output recipe list.");
            FinishNativeSlotUpgrade();
            return;
        }

        std::uintptr_t widgetVtable{};
        SIZE_T bytesRead{};
        if (PendingSlotRequest.widget == nullptr
            || !ReadProcessMemory(GetCurrentProcess(), PendingSlotRequest.widget, &widgetVtable,
                sizeof(widgetVtable), &bytesRead)
            || bytesRead != sizeof(widgetVtable)
            || widgetVtable != MainImageBase + AdvancedStashSlotVtableRva) {
            context->LogWarn("The Crafting slot closed before the game-thread upgrade began; no items were withdrawn.");
            FinishNativeSlotUpgrade();
            return;
        }

        void* const counterItem = GetAdvancedStashSlotCounterItem != nullptr
            ? GetAdvancedStashSlotCounterItem(PendingSlotRequest.widget) : nullptr;
        std::array<std::uint32_t, 4> unitFields{};
        bytesRead = 0;
        if (counterItem == nullptr
            || !ReadProcessMemory(GetCurrentProcess(), counterItem, unitFields.data(), sizeof(unitFields), &bytesRead)
            || bytesRead != sizeof(unitFields)
            || unitFields[0] != 4
            || GetNativeUnitId == nullptr || GetNativeUnitId(counterItem) < 0
            || GetNativeItemCode == nullptr || GetNativeItemCode(counterItem) != PendingSlotRequest.clickedCode) {
            context->LogWarn("The Crafting slot's native item identity did not match the clicked recipe; no items were withdrawn.");
            FinishNativeSlotUpgrade();
            return;
        }

        if (InventoryService->getLocalPlayer(context, &PendingSlotRequest.player) != D2RL::Inventory::Result::Success
            || PendingSlotRequest.player == D2RL::InvalidPlayerHandle) {
            context->LogWarn("Could not resolve the authoritative player for the Crafting-slot upgrade.");
            FinishNativeSlotUpgrade();
            return;
        }
        if (AllowWithoutCube) {
            bool hasHoradricCube{};
            const auto cubeSearchResult = HasHoradricCubeInInventory(context, PendingSlotRequest.player, hasHoradricCube);
            if (cubeSearchResult != D2RL::Inventory::Result::Success) {
                context->LogWarn("Could not determine whether the Horadric Cube is in Inventory; no ingredients were withdrawn.");
                FinishNativeSlotUpgrade();
                return;
            }
            PendingSlotRequest.stagingContainer = hasHoradricCube
                ? D2RL::Items::ItemContainer::Cube : D2RL::Items::ItemContainer::Inventory;
        }
        const std::int32_t dataContext = GetLocalDataContext != nullptr ? GetLocalDataContext() : -1;
        void* const nativePlayer = dataContext >= 0 && GetLocalPlayer != nullptr ? GetLocalPlayer(dataContext) : nullptr;
        void* const stashOwner = nativePlayer != nullptr && GetAdvancedStashDestination != nullptr
            ? GetAdvancedStashDestination(nativePlayer) : nullptr;
        if (stashOwner == nullptr || GetNativeUnitId(stashOwner) < 0
            || RequestAdvancedStashBatchWithdrawal == nullptr || FindAdvancedStashSlotByClass == nullptr
            || GetAdvancedStashCounterByte == nullptr) {
            context->LogWarn("Could not resolve the native Advanced Stash owner; no ingredients were moved.");
            FinishNativeSlotUpgrade();
            return;
        }

        std::vector<std::pair<void*, std::uint8_t>> withdrawalItems;
        withdrawalItems.reserve(PendingSlotRequest.transactionIngredients.size());
        for (const auto& ingredient : PendingSlotRequest.transactionIngredients) {
            const auto itemClass = ItemClassByCode.find(ingredient.code);
            if (itemClass == ItemClassByCode.end() || ingredient.quantity == 0
                || ingredient.quantity > std::numeric_limits<std::uint8_t>::max()) {
                context->LogWarn("A recipe ingredient could not be mapped to a native Advanced Stash item class; no items were withdrawn.");
                FinishNativeSlotUpgrade();
                return;
            }
            void* const stashItem = FindAdvancedStashSlotByClass(stashOwner, itemClass->second);
            if (stashItem == nullptr || GetNativeUnitId(stashItem) < 0
                || GetNativeItemCode(stashItem) != ingredient.code) {
                FinishNativeSlotUpgrade();
                return;
            }
            if (ingredient.code == PendingSlotRequest.clickedCode && stashItem != counterItem) {
                context->LogWarn("The native class lookup did not resolve the clicked rune slot; no items were withdrawn.");
                FinishNativeSlotUpgrade();
                return;
            }
            std::array<std::uint32_t, 4> stashUnitFields{};
            bytesRead = 0;
            if (!ReadProcessMemory(GetCurrentProcess(), stashItem, stashUnitFields.data(), sizeof(stashUnitFields), &bytesRead)
                || bytesRead != sizeof(stashUnitFields) || stashUnitFields[0] != 4) {
                context->LogWarn("A required Crafting slot did not resolve to a validated native item; no items were withdrawn.");
                FinishNativeSlotUpgrade();
                return;
            }
            if (GetAdvancedStashCounterByte(stashItem) < ingredient.quantity) {
                FinishNativeSlotUpgrade();
                return;
            }
            (void)MarkAdvancedStashItemIdentified(stashItem, ingredient.code);
            withdrawalItems.emplace_back(stashItem, static_cast<std::uint8_t>(ingredient.quantity));
        }

        const D2RL::Items::ItemContainer stagingContainer = PendingSlotRequest.stagingContainer;
        const char* const stagingName = stagingContainer == D2RL::Items::ItemContainer::Inventory
            ? "Inventory" : "Cube";
        StashItems stagedItemsBefore{};
        const auto stagingResult = ReadStagingItems(context, PendingSlotRequest.player,
            stagingContainer, stagedItemsBefore);
        if (stagingResult != D2RL::Inventory::Result::Success || stagedItemsBefore.allocationFailed) {
            char message[160]{};
            std::snprintf(message, sizeof(message), "Could not snapshot %s before withdrawal; no ingredients were moved.", stagingName);
            context->LogWarn(message);
            FinishNativeSlotUpgrade();
            return;
        }

        PendingSlotRequest.stagedItemsBefore = std::move(stagedItemsBefore.items);
        PendingSlotRequest.pollCount = 0;
        HideTemporaryInventoryWidgets.store(true, std::memory_order_release);
        PendingSlotRequest.withdrawalSent = true;
        const std::uint8_t withdrawalDestination = stagingContainer == D2RL::Items::ItemContainer::Inventory
            ? AdvancedStashInventoryDestination : AdvancedStashCubeDestination;
        for (const auto& [stashItem, quantity] : withdrawalItems) {
            RequestAdvancedStashBatchWithdrawal(stashItem, stashOwner, withdrawalDestination, quantity);
        }

        if (ThreadService->runOnGameThread(context, PollNativeSlotUpgradeOnGameThread, nullptr)
            != D2RL::Threads::Result::Success) {
            char message[160]{};
            std::snprintf(message, sizeof(message),
                "The withdrawal was sent, but the game thread could not monitor it; check %s before retrying.", stagingName);
            context->LogWarn(message);
            FinishNativeSlotUpgrade();
        }
    } catch (...) {
        context->LogError("Rune Gem Quick Upgrade stopped safely before completing the native withdrawal.");
        FinishNativeSlotUpgrade();
    }
}

auto IsCraftingSlotCode(std::uint32_t itemCode) noexcept -> bool {
    for (const auto& slot : CraftingSlotLayouts) {
        if (slot.itemCode.size() == 3
            && D2RL::Items::MakeItemCode(slot.itemCode[0], slot.itemCode[1], slot.itemCode[2]) == itemCode) {
            return true;
        }
    }
    return false;
}

auto TryHandleAdvancedStashSlotClick(const D2RL::PluginContext* context, void* widget,
    SlotAction action) noexcept -> bool {
    if (context == nullptr || widget == nullptr || ThreadService == nullptr
        || (action != SlotAction::Upgrade && action != SlotAction::Downgrade)
        || GetAdvancedStashSlotCounterItem == nullptr || GetNativeUnitId == nullptr
        || GetAdvancedStashCounterByte == nullptr || GetNativeItemCode == nullptr
        || RequestAdvancedStashBatchWithdrawal == nullptr || GetLocalDataContext == nullptr
        || GetLocalPlayer == nullptr || GetAdvancedStashDestination == nullptr
        || TransferItemToInventoryPage == nullptr || FinishInventoryInteraction == nullptr) {
        return false;
    }

    std::uintptr_t widgetVtable{};
    SIZE_T bytesRead{};
    if (!ReadProcessMemory(GetCurrentProcess(), widget, &widgetVtable, sizeof(widgetVtable), &bytesRead)
        || bytesRead != sizeof(widgetVtable)
        || widgetVtable != MainImageBase + AdvancedStashSlotVtableRva) {
        return false;
    }

    void* const counterItem = GetAdvancedStashSlotCounterItem(widget);
    if (counterItem == nullptr || GetNativeUnitId(counterItem) < 0) {
        return false;
    }
    const std::uint32_t slotCode = GetNativeItemCode(counterItem);
    if (!IsCraftingSlotCode(slotCode)) {
        return false;
    }
    const Recipe* matchedRecipe{};
    for (const auto& recipe : Recipes) {
        if (recipe.inputs.empty() || recipe.outputs.size() != 1) {
            continue;
        }
        // Upgrade clicks use the recipe's primary input. Downgrade clicks use
        // its sole output and recreate all of the original inputs.
        const bool matchesSlot = action == SlotAction::Upgrade
            ? recipe.inputs.front().code == slotCode
            : recipe.outputs.front() == slotCode;
        if (!matchesSlot) {
            continue;
        }
        if (matchedRecipe != nullptr) {
            context->LogWarn("More than one valid one-output recipe matches this Crafting slot; refusing an ambiguous operation.");
            return false;
        }
        matchedRecipe = &recipe;
    }
    if (matchedRecipe == nullptr) {
        return false;
    }
    return QueueNativeSlotOperation(context, widget, slotCode, *matchedRecipe, action);
}

auto __fastcall HandleAdvancedStashSlotInput(void* widget, std::uintptr_t eventState) noexcept -> void {
    const D2RL::PluginContext* const context = LoadContext;
    if (context != nullptr && widget != nullptr && MainImageBase != 0
        && GetAdvancedStashSlotCounterItem != nullptr && GetNativeItemCode != nullptr) {
        std::uintptr_t widgetVtable{};
        SIZE_T bytesRead{};
        if (ReadProcessMemory(GetCurrentProcess(), widget, &widgetVtable, sizeof(widgetVtable), &bytesRead)
            && bytesRead == sizeof(widgetVtable)
            && widgetVtable == MainImageBase + AdvancedStashSlotVtableRva) {
            void* const item = GetAdvancedStashSlotCounterItem(widget);
            const std::uint32_t code = item != nullptr ? GetNativeItemCode(item) : 0;
            if (GemItemCodes.contains(code) || RuneItemCodes.contains(code)) {
                (void)MarkAdvancedStashItemIdentified(item, code);
            }
        }
    }

    const AdvancedStashSlotInputHandler original = OriginalAdvancedStashSlotInputHandler;
    if (original != nullptr) {
        original(widget, eventState);
    }
}

void __cdecl IdentifyLoadedAdvancedStashMaterials(const D2RL::PluginContext* context, void*) noexcept {
    if (context == nullptr || context != LoadContext || MainImageBase == 0
        || GetLocalDataContext == nullptr || GetLocalPlayer == nullptr
        || GetAdvancedStashDestination == nullptr || FindAdvancedStashSlotByClass == nullptr
        || GetNativeUnitId == nullptr || GetNativeItemCode == nullptr
        || AdvancedStashStackableItemCodes.empty()) {
        return;
    }

    const std::int32_t dataContext = GetLocalDataContext();
    void* const nativePlayer = dataContext >= 0 ? GetLocalPlayer(dataContext) : nullptr;
    void* const stashOwner = nativePlayer != nullptr ? GetAdvancedStashDestination(nativePlayer) : nullptr;
    if (nativePlayer == nullptr || stashOwner == nullptr || GetNativeUnitId(nativePlayer) < 0
        || GetNativeUnitId(stashOwner) < 0) {
        return;
    }

    for (const std::uint32_t code : AdvancedStashStackableItemCodes) {
        const auto itemClass = ItemClassByCode.find(code);
        if (itemClass == ItemClassByCode.end()) {
            continue;
        }
        void* const item = FindAdvancedStashSlotByClass(stashOwner, itemClass->second);
        if (item != nullptr && GetNativeItemCode(item) == code) {
            (void)MarkAdvancedStashItemIdentified(item, code);
        }
    }

    // Loaded Advanced Stash stackables may lack D2R's identified bit until a
    // normal item transfer rebuilds their state. Mark only native Normal-quality
    // items before the player opens the stash; other qualities are left alone.
}

void __cdecl OnLocalPlayerReadyForMaterialIdentification(const D2RL::PluginContext* context,
    const D2RL::Lifecycle::GameplayEvent* event, void*) noexcept {
    if (context == nullptr || context != LoadContext
        || !D2RL::Lifecycle::HasGameplayEventField(event, D2RL::Lifecycle::GameplayEventRequiredSize)
        || event->kind != D2RL::Lifecycle::GameplayEventKind::LocalPlayerReady
        || ThreadService == nullptr || ThreadService->runOnGameThread == nullptr
        || OriginalAdvancedStashSlotInputHandler == nullptr) {
        return;
    }

    const auto result = ThreadService->runOnGameThread(context, IdentifyLoadedAdvancedStashMaterials, nullptr);
    if (result != D2RL::Threads::Result::Success) {
        context->LogWarn("Could not queue the Advanced Stash material identification refresh after local-player readiness.");
    }
}

auto InstallStashMaterialIdentificationListener(const D2RL::PluginContext* context) noexcept -> void {
    if (context == nullptr || OriginalAdvancedStashSlotInputHandler == nullptr) {
        return;
    }

    const D2RL::LifecycleService* lifecycle{};
    if (context->QueryService(&lifecycle) != D2RL::ServiceQueryResult::Success
        || !D2RL::HasLifecycleServiceField(lifecycle, D2RL::LifecycleServiceRequiredSize)) {
        context->LogWarn("Automatic Advanced Stash material identification refresh is unavailable because the lifecycle service could not be queried.");
        return;
    }

    const D2RL::Lifecycle::GameplayEventListener listener {
        .structSize = D2RL::Lifecycle::GameplayEventListenerSize,
        .kind       = D2RL::Lifecycle::GameplayEventKind::LocalPlayerReady,
        .callback   = OnLocalPlayerReadyForMaterialIdentification,
    };
    D2RL::Lifecycle::ListenerHandle handle = D2RL::Lifecycle::InvalidHandle;
    if (lifecycle->registerGameplayEventListener(context, &listener, &handle) != D2RL::Lifecycle::Result::Success
        || handle == D2RL::Lifecycle::InvalidHandle) {
        context->LogWarn("Automatic Advanced Stash material identification refresh could not register its local-player-ready listener.");
    }
}

auto __fastcall ConsumeCurrentInputForUpgrade() noexcept -> void {
    const D2RL::PluginContext* context = LoadContext;
    if (context != nullptr && ActiveSlotAction != SlotAction::None && ActiveInputWidget != nullptr) {
        std::uintptr_t widgetVtable{};
        SIZE_T bytesRead{};
        if (ReadProcessMemory(GetCurrentProcess(), ActiveInputWidget, &widgetVtable, sizeof(widgetVtable), &bytesRead)
            && bytesRead == sizeof(widgetVtable)
            && widgetVtable == MainImageBase + AdvancedStashSlotVtableRva) {
            (void)TryHandleAdvancedStashSlotClick(context, ActiveInputWidget, ActiveSlotAction);
        }
    }

    const CurrentInputConsumer original = OriginalCurrentInputConsumer;
    if (original != nullptr) {
        original();
    }
}

auto ModifierKeyBit(std::uint32_t virtualKey, bool altKey) noexcept -> std::uint32_t {
    if (altKey) {
        switch (virtualKey) {
        case VK_MENU: return ModifierGenericKeyBit;
        case VK_LMENU: return ModifierLeftKeyBit;
        case VK_RMENU: return ModifierRightKeyBit;
        default: return 0;
        }
    }
    switch (virtualKey) {
    case VK_SHIFT: return ModifierGenericKeyBit;
    case VK_LSHIFT: return ModifierLeftKeyBit;
    case VK_RSHIFT: return ModifierRightKeyBit;
    default: return 0;
    }
}

auto TrackModifierKeyState(const D2RL::PluginContext*, const D2RL::Input::KeyEvent* event,
    void*) noexcept -> D2RL::Input::KeyEventResult {
    if (event == nullptr || event->structSize < D2RL::Input::KeyEventSize
        || (event->kind != D2RL::Input::KeyEventKind::Down
            && event->kind != D2RL::Input::KeyEventKind::Up)) {
        return D2RL::Input::KeyEventResult::Continue;
    }
    const std::uint32_t altBit = ModifierKeyBit(event->virtualKey, true);
    const std::uint32_t shiftBit = ModifierKeyBit(event->virtualKey, false);
    auto updateState = [event](std::atomic_uint32_t& state, std::uint32_t bit) noexcept {
        if (bit == 0) {
            return;
        }
        if (event->kind == D2RL::Input::KeyEventKind::Down) {
            state.fetch_or(bit, std::memory_order_release);
        } else {
            state.fetch_and(~bit, std::memory_order_release);
        }
    };
    updateState(AltKeyState, altBit);
    updateState(ShiftKeyState, shiftBit);
    return D2RL::Input::KeyEventResult::Continue;
}

auto InstallModifierKeyListener(const D2RL::PluginContext* context) noexcept -> void {
    if (context == nullptr) {
        return;
    }
    if (context->QueryService(&InputApiService) != D2RL::ServiceQueryResult::Success
        || !D2RL::HasInputServiceField(InputApiService, D2RL::InputServiceRegisterKeyListenerFieldEnd)
        || InputApiService->registerKeyListener == nullptr) {
        InputApiService = nullptr;
        context->LogWarn("Modifier key listener is unavailable; using the Windows keyboard-state fallback.");
        return;
    }

    const D2RL::Input::KeyListenerRegistration registration {
        .structSize = D2RL::Input::KeyListenerRegistrationSize,
        .flags = 0,
        .callback = TrackModifierKeyState,
        .userData = nullptr,
    };
    if (InputApiService->registerKeyListener(context, &registration, &ModifierKeyListener)
            != D2RL::Input::Result::Success
        || ModifierKeyListener == D2RL::Input::InvalidKeyListenerHandle) {
        ModifierKeyListener = D2RL::Input::InvalidKeyListenerHandle;
        InputApiService = nullptr;
        context->LogWarn("Modifier key listener could not be registered; using the Windows keyboard-state fallback.");
        return;
    }
    ModifierKeyListenerActive.store(true, std::memory_order_release);
}

auto __fastcall BridgeItemWidgetInput(void* widget, const void* message) noexcept -> void {
    bool shiftDown{};
    bool altDown{};
    if (ModifierKeyListenerActive.load(std::memory_order_acquire)) {
        shiftDown = ShiftKeyState.load(std::memory_order_acquire) != 0;
        altDown = AltKeyState.load(std::memory_order_acquire) != 0;
    } else {
        shiftDown = (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;
        altDown = (GetAsyncKeyState(VK_MENU) & 0x8000) != 0;
    }
    std::uint64_t commandHash{};
    SIZE_T bytesRead{};
    SlotAction slotAction{SlotAction::None};
    if (shiftDown != altDown && message != nullptr) {
        const auto* messageBytes = static_cast<const std::uint8_t*>(message);
        if (ReadProcessMemory(GetCurrentProcess(), messageBytes + 0x88, &commandHash, sizeof(commandHash), &bytesRead)
            && bytesRead == sizeof(commandHash)
            && commandHash == AdvancedStashRightClickCommand) {
            slotAction = shiftDown ? SlotAction::Upgrade : SlotAction::Downgrade;
        }
    }

    const SlotAction previousSlotAction = ActiveSlotAction;
    void* const previousInputWidget = ActiveInputWidget;
    ActiveSlotAction = slotAction;
    ActiveInputWidget = widget;
    const ItemWidgetInputHandler original = OriginalItemWidgetInputHandler;
    if (original != nullptr) {
        original(widget, message);
    }
    ActiveSlotAction = previousSlotAction;
    ActiveInputWidget = previousInputWidget;
}

auto InstallItemWidgetInputBridge(const D2RL::PluginContext* context) noexcept -> void {
    if (context == nullptr || !context->CheckExpectedBytes(ItemWidgetInputDispatcherRva,
            ItemWidgetInputDispatcherExpected.data(), static_cast<std::uint32_t>(ItemWidgetInputDispatcherExpected.size()))) {
        if (context != nullptr) {
            context->LogWarn("Native input bridge skipped: its build fingerprint did not match.");
        }
        return;
    }
    const D2RL::DiagnosticsService* diagnostics{};
    if (context->QueryService(&diagnostics) == D2RL::ServiceQueryResult::Success
        && D2RL::HasDiagnosticsServiceField(diagnostics, D2RL::DiagnosticsServiceRequiredSize)
        && diagnostics->queryHookStatus != nullptr) {
        const D2RL::Diagnostics::HookQuery query {
            .structSize = D2RL::Diagnostics::HookQuerySize,
            .rva = ItemWidgetInputDispatcherRva,
            .expected = ItemWidgetInputDispatcherExpected.data(),
            .expectedSize = static_cast<std::uint32_t>(ItemWidgetInputDispatcherExpected.size()),
        };
        D2RL::Diagnostics::HookStatus status {
            .structSize = D2RL::Diagnostics::HookStatusSize,
        };
        const auto result = diagnostics->queryHookStatus(context, &query, &status);
        if (result == D2RL::Diagnostics::Result::Success
            && status.state != D2RL::Diagnostics::ModificationState::Unchanged) {
            context->LogWarn("Native input bridge skipped: another owner already modifies the dispatcher.");
            return;
        }
    }
    if (!context->InstallInlineHook(ItemWidgetInputDispatcherRva, ItemWidgetInputDispatcherExpected.data(),
            static_cast<std::uint32_t>(ItemWidgetInputDispatcherExpected.size()),
            reinterpret_cast<ItemWidgetInputHandler>(BridgeItemWidgetInput), &OriginalItemWidgetInputHandler)) {
        OriginalItemWidgetInputHandler = nullptr;
        context->LogWarn("Native input bridge could not be installed.");
        return;
    }
}

auto InstallConsumeCurrentInputHook(const D2RL::PluginContext* context) noexcept -> void {
    if (context == nullptr || MainImageBase == 0
        || !context->CheckExpectedBytes(ConsumeCurrentInputRva, ConsumeCurrentInputExpected.data(),
            static_cast<std::uint32_t>(ConsumeCurrentInputExpected.size()))) {
        if (context != nullptr) {
            context->LogWarn("Hit-tested input consume hook skipped: the current UI_ConsumeCurrentInput fingerprint did not match.");
        }
        return;
    }

    const D2RL::DiagnosticsService* diagnostics{};
    if (context->QueryService(&diagnostics) == D2RL::ServiceQueryResult::Success
        && D2RL::HasDiagnosticsServiceField(diagnostics, D2RL::DiagnosticsServiceRequiredSize)
        && diagnostics->queryHookStatus != nullptr) {
        const D2RL::Diagnostics::HookQuery query {
            .structSize = D2RL::Diagnostics::HookQuerySize,
            .rva = ConsumeCurrentInputRva,
            .expected = ConsumeCurrentInputExpected.data(),
            .expectedSize = static_cast<std::uint32_t>(ConsumeCurrentInputExpected.size()),
        };
        D2RL::Diagnostics::HookStatus status {
            .structSize = D2RL::Diagnostics::HookStatusSize,
        };
        const auto result = diagnostics->queryHookStatus(context, &query, &status);
        if (result == D2RL::Diagnostics::Result::Success
            && status.state != D2RL::Diagnostics::ModificationState::Unchanged) {
            context->LogWarn("Hit-tested input consume hook skipped: another owner already modifies UI_ConsumeCurrentInput.");
            return;
        }
    }

    if (!context->InstallInlineHook(ConsumeCurrentInputRva, ConsumeCurrentInputExpected.data(),
            static_cast<std::uint32_t>(ConsumeCurrentInputExpected.size()),
            reinterpret_cast<CurrentInputConsumer>(ConsumeCurrentInputForUpgrade), &OriginalCurrentInputConsumer)) {
        OriginalCurrentInputConsumer = nullptr;
        context->LogWarn("Hit-tested input consume hook could not be installed.");
        return;
    }
}

auto ValidateAdvancedStashSlotVtable() noexcept -> bool {
    MainImageBase = reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
    if (MainImageBase == 0) {
        return false;
    }
    std::uintptr_t actualCounterGetter{};
    std::uintptr_t actualInputHandler{};
    SIZE_T bytesRead{};
    const auto* counterGetterEntry = reinterpret_cast<const void*>(MainImageBase + AdvancedStashSlotVtableRva + 0xC8);
    if (!ReadProcessMemory(GetCurrentProcess(), counterGetterEntry, &actualCounterGetter, sizeof(actualCounterGetter), &bytesRead)
        || bytesRead != sizeof(actualCounterGetter)) {
        return false;
    }
    bytesRead = 0;
    const auto* inputHandlerEntry = reinterpret_cast<const void*>(MainImageBase + AdvancedStashSlotVtableRva + 0x100);
    if (!ReadProcessMemory(GetCurrentProcess(), inputHandlerEntry, &actualInputHandler, sizeof(actualInputHandler), &bytesRead)
        || bytesRead != sizeof(actualInputHandler)) {
        return false;
    }
    return actualCounterGetter == MainImageBase + AdvancedStashSlotCounterGetterRva
        && actualInputHandler == MainImageBase + AdvancedStashSlotInputRva;
}

auto InstallTemporarySpriteRendererHook(const D2RL::PluginContext* context) noexcept -> void {
    if (context == nullptr || !context->CheckExpectedBytes(InventoryItemRendererRva,
            InventoryItemRendererExpected.data(), static_cast<std::uint32_t>(InventoryItemRendererExpected.size()))) {
        if (context != nullptr) {
            context->LogWarn("Temporary material sprite suppression skipped: the live item renderer fingerprint did not match.");
        }
        return;
    }

    const D2RL::DiagnosticsService* diagnostics{};
    if (context->QueryService(&diagnostics) == D2RL::ServiceQueryResult::Success
        && D2RL::HasDiagnosticsServiceField(diagnostics, D2RL::DiagnosticsServiceRequiredSize)
        && diagnostics->queryHookStatus != nullptr) {
        const D2RL::Diagnostics::HookQuery query {
            .structSize = D2RL::Diagnostics::HookQuerySize,
            .rva = InventoryItemRendererRva,
            .expected = InventoryItemRendererExpected.data(),
            .expectedSize = static_cast<std::uint32_t>(InventoryItemRendererExpected.size()),
        };
        D2RL::Diagnostics::HookStatus status {
            .structSize = D2RL::Diagnostics::HookStatusSize,
        };
        const auto result = diagnostics->queryHookStatus(context, &query, &status);
        if (result == D2RL::Diagnostics::Result::Success
            && status.state != D2RL::Diagnostics::ModificationState::Unchanged) {
            context->LogWarn("Temporary material sprite suppression skipped: another owner already modifies the item renderer.");
            return;
        }
    }

    if (!context->InstallInlineHook(InventoryItemRendererRva, InventoryItemRendererExpected.data(),
            static_cast<std::uint32_t>(InventoryItemRendererExpected.size()),
            reinterpret_cast<InventoryItemRenderer>(DrawInventoryItemWithTemporarySpritesHidden),
            &OriginalInventoryItemRenderer)) {
        OriginalInventoryItemRenderer = nullptr;
        context->LogWarn("Temporary material sprite renderer hook could not be installed.");
        return;
    }
}

auto ValidateInventoryItemWidgetDrawVtable() noexcept -> bool {
    if (MainImageBase == 0) {
        return false;
    }
    std::uintptr_t drawFunction{};
    SIZE_T bytesRead{};
    const auto* entry = reinterpret_cast<const void*>(MainImageBase + InventoryItemWidgetVtableRva + 0xF0);
    return ReadProcessMemory(GetCurrentProcess(), entry, &drawFunction, sizeof(drawFunction), &bytesRead)
        && bytesRead == sizeof(drawFunction)
        && drawFunction == MainImageBase + InventoryItemWidgetDrawRva;
}

auto InstallTemporaryInventoryWidgetDrawHook(const D2RL::PluginContext* context) noexcept -> void {
    if (context == nullptr || !ValidateInventoryItemWidgetDrawVtable()
        || !context->CheckExpectedBytes(InventoryItemWidgetDrawRva,
            InventoryItemWidgetDrawExpected.data(), static_cast<std::uint32_t>(InventoryItemWidgetDrawExpected.size()))) {
        if (context != nullptr) {
            context->LogWarn("Temporary inventory-cell flash suppression skipped: the live item-widget draw fingerprint or vtable did not match.");
        }
        return;
    }

    const D2RL::DiagnosticsService* diagnostics{};
    if (context->QueryService(&diagnostics) == D2RL::ServiceQueryResult::Success
        && D2RL::HasDiagnosticsServiceField(diagnostics, D2RL::DiagnosticsServiceRequiredSize)
        && diagnostics->queryHookStatus != nullptr) {
        const D2RL::Diagnostics::HookQuery query {
            .structSize = D2RL::Diagnostics::HookQuerySize,
            .rva = InventoryItemWidgetDrawRva,
            .expected = InventoryItemWidgetDrawExpected.data(),
            .expectedSize = static_cast<std::uint32_t>(InventoryItemWidgetDrawExpected.size()),
        };
        D2RL::Diagnostics::HookStatus status {
            .structSize = D2RL::Diagnostics::HookStatusSize,
        };
        const auto result = diagnostics->queryHookStatus(context, &query, &status);
        if (result == D2RL::Diagnostics::Result::Success
            && status.state != D2RL::Diagnostics::ModificationState::Unchanged) {
            context->LogWarn("Temporary inventory-cell flash suppression skipped: another owner already modifies the item-widget draw method.");
            return;
        }
    }

    if (!context->InstallInlineHook(InventoryItemWidgetDrawRva, InventoryItemWidgetDrawExpected.data(),
            static_cast<std::uint32_t>(InventoryItemWidgetDrawExpected.size()),
            reinterpret_cast<InventoryItemWidgetDraw>(DrawInventoryItemWidgetWithTemporarySlotsHidden),
            &OriginalInventoryItemWidgetDraw)) {
        OriginalInventoryItemWidgetDraw = nullptr;
        context->LogWarn("Temporary inventory-cell flash suppression hook could not be installed; item sprites will still be hidden.");
        return;
    }
}

auto ValidateInventorySlotWidgetDrawVtable() noexcept -> bool {
    if (MainImageBase == 0) {
        return false;
    }
    std::uintptr_t drawFunction{};
    SIZE_T bytesRead{};
    const auto* entry = reinterpret_cast<const void*>(MainImageBase + InventorySlotWidgetVtableRva + 0xF0);
    return ReadProcessMemory(GetCurrentProcess(), entry, &drawFunction, sizeof(drawFunction), &bytesRead)
        && bytesRead == sizeof(drawFunction)
        && drawFunction == MainImageBase + InventorySlotWidgetDrawRva;
}

auto InstallTemporaryInventorySlotWidgetDrawHook(const D2RL::PluginContext* context) noexcept -> void {
    if (context == nullptr || !ValidateInventorySlotWidgetDrawVtable()
        || !context->CheckExpectedBytes(InventorySlotWidgetDrawRva,
            InventorySlotWidgetDrawExpected.data(), static_cast<std::uint32_t>(InventorySlotWidgetDrawExpected.size()))) {
        if (context != nullptr) {
            context->LogWarn("Temporary InventorySlotWidget suppression skipped: the live slot vtable or draw fingerprint did not match.");
        }
        return;
    }

    const D2RL::DiagnosticsService* diagnostics{};
    if (context->QueryService(&diagnostics) == D2RL::ServiceQueryResult::Success
        && D2RL::HasDiagnosticsServiceField(diagnostics, D2RL::DiagnosticsServiceRequiredSize)
        && diagnostics->queryHookStatus != nullptr) {
        const D2RL::Diagnostics::HookQuery query {
            .structSize = D2RL::Diagnostics::HookQuerySize,
            .rva = InventorySlotWidgetDrawRva,
            .expected = InventorySlotWidgetDrawExpected.data(),
            .expectedSize = static_cast<std::uint32_t>(InventorySlotWidgetDrawExpected.size()),
        };
        D2RL::Diagnostics::HookStatus status {
            .structSize = D2RL::Diagnostics::HookStatusSize,
        };
        const auto result = diagnostics->queryHookStatus(context, &query, &status);
        if (result == D2RL::Diagnostics::Result::Success
            && status.state != D2RL::Diagnostics::ModificationState::Unchanged) {
            context->LogWarn("Temporary InventorySlotWidget suppression skipped: another owner already modifies the shared draw method.");
            return;
        }
    }

    if (!context->InstallInlineHook(InventorySlotWidgetDrawRva, InventorySlotWidgetDrawExpected.data(),
            static_cast<std::uint32_t>(InventorySlotWidgetDrawExpected.size()),
            reinterpret_cast<InventorySlotWidgetDraw>(DrawInventorySlotWidgetWithTemporarySlotsHidden),
            &OriginalInventorySlotWidgetDraw)) {
        OriginalInventorySlotWidgetDraw = nullptr;
        context->LogWarn("Temporary InventorySlotWidget suppression hook could not be installed; slot cells may still flash.");
        return;
    }
}

auto InstallAdvancedStashSlotInputHook(const D2RL::PluginContext* context) noexcept -> void {
    if (context == nullptr
        || !context->CheckExpectedBytes(AdvancedStashSlotInputRva, AdvancedStashSlotInputExpected.data(),
            static_cast<std::uint32_t>(AdvancedStashSlotInputExpected.size()))
        || !context->CheckExpectedBytes(AdvancedStashSlotCounterGetterRva, AdvancedStashSlotCounterGetterExpected.data(),
            static_cast<std::uint32_t>(AdvancedStashSlotCounterGetterExpected.size()))
        || !context->CheckExpectedBytes(NativeGetUnitIdRva, NativeGetUnitIdExpected.data(),
            static_cast<std::uint32_t>(NativeGetUnitIdExpected.size()))
        || !context->CheckExpectedBytes(AdvancedStashCounterByteWitnessRva, AdvancedStashCounterByteExpected.data(),
            static_cast<std::uint32_t>(AdvancedStashCounterByteExpected.size()))
        || !context->CheckExpectedBytes(AdvancedStashBatchWithdrawalRva, AdvancedStashBatchWithdrawalExpected.data(),
            static_cast<std::uint32_t>(AdvancedStashBatchWithdrawalExpected.size()))
        || !context->CheckExpectedBytes(NativeGetLocalDataContextRva, NativeGetLocalDataContextExpected.data(),
            static_cast<std::uint32_t>(NativeGetLocalDataContextExpected.size()))
        || !context->CheckExpectedBytes(NativeGetLocalPlayerRva, NativeGetLocalPlayerExpected.data(),
            static_cast<std::uint32_t>(NativeGetLocalPlayerExpected.size()))
        || !context->CheckExpectedBytes(NativeGetItemCodeRva, NativeGetItemCodeExpected.data(),
            static_cast<std::uint32_t>(NativeGetItemCodeExpected.size()))
        || !context->CheckExpectedBytes(NativeGetAdvancedStashDestinationRva, NativeGetAdvancedStashDestinationExpected.data(),
            static_cast<std::uint32_t>(NativeGetAdvancedStashDestinationExpected.size()))
        || !context->CheckExpectedBytes(NativeTransferItemToInventoryPageRva, NativeTransferItemToInventoryPageExpected.data(),
            static_cast<std::uint32_t>(NativeTransferItemToInventoryPageExpected.size()))
        || !context->CheckExpectedBytes(NativeFinishInventoryInteractionRva, NativeFinishInventoryInteractionExpected.data(),
            static_cast<std::uint32_t>(NativeFinishInventoryInteractionExpected.size()))
        || !context->CheckExpectedBytes(NativeFindAdvancedStashSlotByClassRva, NativeFindAdvancedStashSlotByClassExpected.data(),
            static_cast<std::uint32_t>(NativeFindAdvancedStashSlotByClassExpected.size()))
        || !ValidateAdvancedStashSlotVtable()) {
        if (context != nullptr) {
            context->LogWarn("Advanced Stash upgrade hook skipped: a required game function fingerprint or slot vtable did not match.");
        }
        AttachSound = nullptr;
        MainImageBase = 0;
        return;
    }

    if (context->CheckExpectedBytes(NativeAttachSoundRva, NativeAttachSoundExpected.data(),
            static_cast<std::uint32_t>(NativeAttachSoundExpected.size()))) {
        AttachSound = reinterpret_cast<NativeAttachSound>(MainImageBase + NativeAttachSoundRva);
    } else {
        AttachSound = nullptr;
        context->LogWarn("Gem-upgrade sound is disabled because the native sound function fingerprint did not match.");
    }

    const D2RL::DiagnosticsService* diagnostics{};
    if (context->QueryService(&diagnostics) == D2RL::ServiceQueryResult::Success
        && D2RL::HasDiagnosticsServiceField(diagnostics, D2RL::DiagnosticsServiceRequiredSize)
        && diagnostics->queryHookStatus != nullptr) {
        const D2RL::Diagnostics::HookQuery query {
            .structSize = D2RL::Diagnostics::HookQuerySize,
            .rva = AdvancedStashSlotInputRva,
            .expected = AdvancedStashSlotInputExpected.data(),
            .expectedSize = static_cast<std::uint32_t>(AdvancedStashSlotInputExpected.size()),
        };
        D2RL::Diagnostics::HookStatus status {
            .structSize = D2RL::Diagnostics::HookStatusSize,
        };
        const auto result = diagnostics->queryHookStatus(context, &query, &status);
        if (result == D2RL::Diagnostics::Result::Success
            && status.state != D2RL::Diagnostics::ModificationState::Unchanged) {
            context->LogWarn("Advanced stash slot hook skipped: the handler already has another modification owner.");
            MainImageBase = 0;
            return;
        }
    }

    GetAdvancedStashSlotCounterItem = reinterpret_cast<AdvancedStashSlotCounterGetter>(MainImageBase + AdvancedStashSlotCounterGetterRva);
    GetNativeUnitId = reinterpret_cast<NativeGetUnitId>(MainImageBase + NativeGetUnitIdRva);
    GetAdvancedStashCounterByte = reinterpret_cast<AdvancedStashCounterByte>(MainImageBase + AdvancedStashCounterByteRva);
    GetLocalDataContext = reinterpret_cast<NativeGetLocalDataContext>(MainImageBase + NativeGetLocalDataContextRva);
    GetLocalPlayer = reinterpret_cast<NativeGetLocalPlayer>(MainImageBase + NativeGetLocalPlayerRva);
    GetNativeItemCode = reinterpret_cast<NativeGetItemCode>(MainImageBase + NativeGetItemCodeRva);
    GetAdvancedStashDestination = reinterpret_cast<NativeGetAdvancedStashDestination>(MainImageBase + NativeGetAdvancedStashDestinationRva);
    RequestAdvancedStashBatchWithdrawal = reinterpret_cast<NativeBatchWithdrawal>(MainImageBase + AdvancedStashBatchWithdrawalRva);
    TransferItemToInventoryPage = reinterpret_cast<NativeTransferItemToInventoryPage>(MainImageBase + NativeTransferItemToInventoryPageRva);
    FinishInventoryInteraction = reinterpret_cast<NativeFinishInventoryInteraction>(MainImageBase + NativeFinishInventoryInteractionRva);
    FindAdvancedStashSlotByClass = reinterpret_cast<NativeFindAdvancedStashSlotByClass>(MainImageBase + NativeFindAdvancedStashSlotByClassRva);
    if (!context->InstallInlineHook(AdvancedStashSlotInputRva, AdvancedStashSlotInputExpected.data(),
            static_cast<std::uint32_t>(AdvancedStashSlotInputExpected.size()),
            reinterpret_cast<AdvancedStashSlotInputHandler>(HandleAdvancedStashSlotInput),
            &OriginalAdvancedStashSlotInputHandler)) {
        GetAdvancedStashSlotCounterItem = nullptr;
        GetNativeUnitId = nullptr;
        GetAdvancedStashCounterByte = nullptr;
        GetLocalDataContext = nullptr;
        GetLocalPlayer = nullptr;
        GetNativeItemCode = nullptr;
        GetAdvancedStashDestination = nullptr;
        RequestAdvancedStashBatchWithdrawal = nullptr;
        TransferItemToInventoryPage = nullptr;
        FinishInventoryInteraction = nullptr;
        FindAdvancedStashSlotByClass = nullptr;
        AttachSound = nullptr;
        MainImageBase = 0;
        context->LogWarn("Advanced stash slot hook could not be installed.");
        return;
    }
    InstallTemporarySpriteRendererHook(context);
    InstallTemporaryInventoryWidgetDrawHook(context);
    InstallTemporaryInventorySlotWidgetDrawHook(context);
}

} // namespace

D2RL_PLUGIN_EXPORT auto D2RLoaderGetPluginInfo() noexcept -> const D2RL::PluginInfo* {
    return &PluginInfo;
}

D2RL_PLUGIN_EXPORT auto D2RLoaderLoadPlugin(const D2RL::PluginContext* context) noexcept -> bool {
    if (context == nullptr) {
        return false;
    }
    try {
        RequestInFlight.store(false, std::memory_order_release);
        HideTemporaryInventoryWidgets.store(false, std::memory_order_release);
        ClearTemporarySpriteCodes();
        AllowWithoutCube = false;
        PendingSlotRequest = PendingSlotUpgrade{};
        ActiveSlotAction = SlotAction::None;
        ActiveInputWidget = nullptr;
        InputApiService = nullptr;
        ModifierKeyListener = D2RL::Input::InvalidKeyListenerHandle;
        ModifierKeyListenerActive.store(false, std::memory_order_release);
        AltKeyState.store(0, std::memory_order_release);
        ShiftKeyState.store(0, std::memory_order_release);
        OriginalAdvancedStashSlotInputHandler = nullptr;
        OriginalItemWidgetInputHandler = nullptr;
        OriginalCurrentInputConsumer = nullptr;
        OriginalInventoryItemRenderer = nullptr;
        OriginalInventoryItemWidgetDraw = nullptr;
        OriginalInventorySlotWidgetDraw = nullptr;
        GetAdvancedStashSlotCounterItem = nullptr;
        GetNativeUnitId = nullptr;
        GetAdvancedStashCounterByte = nullptr;
        GetLocalDataContext = nullptr;
        GetLocalPlayer = nullptr;
        GetNativeItemCode = nullptr;
        GetAdvancedStashDestination = nullptr;
        RequestAdvancedStashBatchWithdrawal = nullptr;
        TransferItemToInventoryPage = nullptr;
        FinishInventoryInteraction = nullptr;
        FindAdvancedStashSlotByClass = nullptr;
        AttachSound = nullptr;
        MainImageBase = 0;
        GemUpgradeSoundId = 0;
        LoggedGemUpgradeSoundUnavailable.store(false, std::memory_order_relaxed);
        LoadPluginConfig(context);
        if (!LoadRecipes(context)) {
            context->LogWarn("Rune Gem Quick Upgrade loaded without any usable active-mod recipes.");
        }

        if (context->QueryService(&InventoryService) != D2RL::ServiceQueryResult::Success
            || !D2RL::HasInventoryServiceField(InventoryService, D2RL::InventoryServiceRequiredSize)
            || context->QueryService(&ItemService) != D2RL::ServiceQueryResult::Success
            || !D2RL::HasItemServiceField(ItemService, D2RL::ItemServiceRequiredSize)
            || !D2RL::HasItemServiceCapability(ItemService, D2RL::Items::ItemServiceCapability::SharedStashWrite)
            || context->QueryService(&ThreadService) != D2RL::ServiceQueryResult::Success
            || !D2RL::HasThreadServiceField(ThreadService, D2RL::ThreadServiceRequiredSize)) {
            context->LogWarn("Rune Gem Quick Upgrade requires inventory, shared-stash item transaction, and game-thread services.");
            return false;
        }

        LoadContext = context;
        InstallModifierKeyListener(context);
        InstallAdvancedStashSlotInputHook(context);
        InstallStashMaterialIdentificationListener(context);
        InstallItemWidgetInputBridge(context);
        InstallConsumeCurrentInputHook(context);
        return true;
    } catch (...) {
        context->LogError("Rune Gem Quick Upgrade could not initialize safely.");
        return false;
    }
}

D2RL_PLUGIN_EXPORT void D2RLoaderUnloadPlugin() noexcept {
    const D2RL::PluginContext* const context = LoadContext;
    if (context != nullptr && InputApiService != nullptr
        && ModifierKeyListener != D2RL::Input::InvalidKeyListenerHandle
        && D2RL::HasInputServiceField(InputApiService, D2RL::InputServiceUnregisterKeyListenerFieldEnd)
        && InputApiService->unregisterKeyListener != nullptr) {
        (void)InputApiService->unregisterKeyListener(context, ModifierKeyListener);
    }
    ModifierKeyListenerActive.store(false, std::memory_order_release);
    AltKeyState.store(0, std::memory_order_release);
    ShiftKeyState.store(0, std::memory_order_release);
    LoadContext = nullptr;
    AllowWithoutCube = false;
    OriginalAdvancedStashSlotInputHandler = nullptr;
    OriginalItemWidgetInputHandler = nullptr;
    OriginalCurrentInputConsumer = nullptr;
    OriginalInventoryItemRenderer = nullptr;
    OriginalInventoryItemWidgetDraw = nullptr;
    OriginalInventorySlotWidgetDraw = nullptr;
    GetAdvancedStashSlotCounterItem = nullptr;
    GetNativeUnitId = nullptr;
    GetAdvancedStashCounterByte = nullptr;
    GetLocalDataContext = nullptr;
    GetLocalPlayer = nullptr;
    GetNativeItemCode = nullptr;
    GetAdvancedStashDestination = nullptr;
    RequestAdvancedStashBatchWithdrawal = nullptr;
    TransferItemToInventoryPage = nullptr;
    FinishInventoryInteraction = nullptr;
    FindAdvancedStashSlotByClass = nullptr;
    AttachSound = nullptr;
    MainImageBase = 0;
    InventoryService = nullptr;
    ItemService = nullptr;
    ThreadService = nullptr;
    InputApiService = nullptr;
    ModifierKeyListener = D2RL::Input::InvalidKeyListenerHandle;
    Recipes.clear();
    ItemClassByCode.clear();
    AdvancedStashStackableItemCodes.clear();
    GemItemCodes.clear();
    RuneItemCodes.clear();
    GemUpgradeSoundId = 0;
    LoggedGemUpgradeSoundUnavailable.store(false, std::memory_order_relaxed);
    PendingSlotRequest = PendingSlotUpgrade{};
    ClearTemporarySpriteCodes();
    HideTemporaryInventoryWidgets.store(false, std::memory_order_release);
    RequestInFlight.store(false, std::memory_order_release);
}
