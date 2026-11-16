#include "Hooks.h"

#include "../Settings.h"
#include "TrueHud.h"

namespace Stamina::Hooks {

    namespace {

        constexpr std::size_t kUpdateIndex = 0xAD;
        constexpr RE::FormID kOutOfStaminaEffect = 0x080C;
        constexpr RE::FormID kStaminaRecoverySpell = 0x0A57;
        constexpr auto kPluginName = "For Honor Stamina System.esp";

        struct ActorStaminaState {
            bool wasOutOfStamina = false;
        };

        std::unordered_map<RE::Actor*, ActorStaminaState> actorStaminaStates;

        bool HasOutOfStaminaEffect(RE::Actor* actor) {
            if (!actor) {
                return false;
            }

            auto* magicTarget = actor->AsMagicTarget();
            if (!magicTarget) {
                return false;
            }

            auto* effects = magicTarget->GetActiveEffectList();
            if (!effects) {
                return false;
            }

            auto* dataHandler = RE::TESDataHandler::GetSingleton();
            if (!dataHandler) {
                return false;
            }

            auto* effectSetting = dataHandler->LookupForm<RE::EffectSetting>(kOutOfStaminaEffect, kPluginName);

            if (!effectSetting) {
                return false;
            }

            for (auto* activeEffect : *effects) {
                if (!activeEffect || !activeEffect->effect) {
                    continue;
                }

                if (activeEffect->effect->baseEffect == effectSetting) {
                    return true;
                }
            }

            return false;
        }

        void UpdateActorExhaustion(RE::Actor* actor) {
            if (!actor) {
                return;
            }

            auto& state = actorStaminaStates[actor];
            const bool exhausted = HasOutOfStaminaEffect(actor);

            if (exhausted) {
                if (!state.wasOutOfStamina) {
                    state.wasOutOfStamina = true;

                    if (Settings::debugLogging) {
                        logger::info("[Stamina] OUT OF STAMINA START: actor={:08X}", actor->GetFormID());
                    }
                }

                return;
            }

            if (!state.wasOutOfStamina) {
                return;
            }

            const float recoveryAmount = Settings::exhaustionRecoveryPercent;

            if (Settings::debugLogging) {
                logger::info("[Stamina] OUT OF STAMINA END: actor={:08X}, recovery={:.1f}", actor->GetFormID(),
                             recoveryAmount);
            }

            auto* dataHandler = RE::TESDataHandler::GetSingleton();

            if (dataHandler) {
                auto* spell = dataHandler->LookupForm<RE::SpellItem>(kStaminaRecoverySpell, kPluginName);

                if (spell) {
                    auto* caster = actor->GetMagicCaster(RE::MagicSystem::CastingSource::kInstant);

                    if (caster) {
                        caster->CastSpellImmediate(reinterpret_cast<RE::MagicItem*>(spell), true, actor, 1.0f, false,
                                                   recoveryAmount, actor);

                        if (Settings::debugLogging) {
                            logger::info("[Stamina] STAMINA RECOVERY: actor={:08X}, amount={:.1f}", actor->GetFormID(),
                                         recoveryAmount);
                        }
                    } else if (Settings::debugLogging) {
                        logger::error("[Stamina] Failed to obtain instant MagicCaster for actor {:08X}.",
                                      actor->GetFormID());
                    }
                } else if (Settings::debugLogging) {
                    logger::error("[Stamina] Failed to find Stamina Recovery spell.");
                }
            }

            state.wasOutOfStamina = false;
            actorStaminaStates.erase(actor);
        }

        struct ActorUpdateHook {
            using func_t = void (*)(RE::Actor*, float);

            static void ActorThunk(RE::Actor* a_this, float a_delta) {
                if (!a_this) {
                    return;
                }

                actorFunc(a_this, a_delta);
                UpdateActorExhaustion(a_this);
            }

            static void PlayerThunk(RE::Actor* a_this, float a_delta) {
                if (!a_this) {
                    return;
                }

                playerFunc(a_this, a_delta);
                UpdateActorExhaustion(a_this);

                if (a_this == RE::PlayerCharacter::GetSingleton()) {
                    TrueHUD::Update(RE::PlayerCharacter::GetSingleton(), a_delta);
                }
            }

            static inline func_t actorFunc = nullptr;
            static inline func_t playerFunc = nullptr;
        };

    }

    void Install() {
        auto* player = RE::PlayerCharacter::GetSingleton();

        if (!player) {
            logger::error("[Stamina] Actor::Update hook FAILED: PlayerCharacter is null.");
            return;
        }

        REL::Relocation<std::uintptr_t> actorVTable{RE::VTABLE_Actor[0]};

        auto actorUpdateOriginal =
            actorVTable.write_vfunc(kUpdateIndex, reinterpret_cast<std::uintptr_t>(ActorUpdateHook::ActorThunk));

        if (!actorUpdateOriginal) {
            logger::error("[Stamina] Failed to install Actor vtable Update hook.");
            return;
        }

        ActorUpdateHook::actorFunc = reinterpret_cast<ActorUpdateHook::func_t>(actorUpdateOriginal);

        logger::info("[Stamina] Actor::Update hook installed at Actor vtable slot 0x{:X}.", kUpdateIndex);

        const auto actualVTable = *reinterpret_cast<std::uintptr_t**>(player);

        if (!actualVTable) {
            logger::error("[Stamina] Player actual vtable is null.");
            return;
        }

        logger::info("[Stamina] Player actual vtable = {:016X}", reinterpret_cast<std::uintptr_t>(actualVTable));

        REL::Relocation<std::uintptr_t> playerVTable{reinterpret_cast<std::uintptr_t>(actualVTable)};

        auto playerUpdateOriginal =
            playerVTable.write_vfunc(kUpdateIndex, reinterpret_cast<std::uintptr_t>(ActorUpdateHook::PlayerThunk));

        if (!playerUpdateOriginal) {
            logger::error("[Stamina] Failed to install Player Actor::Update hook.");
            return;
        }

        ActorUpdateHook::playerFunc = reinterpret_cast<ActorUpdateHook::func_t>(playerUpdateOriginal);

        logger::info("[Stamina] Player Actor::Update hook installed at slot 0x{:X}.", kUpdateIndex);
    }

    void Update(float delta) {}

}