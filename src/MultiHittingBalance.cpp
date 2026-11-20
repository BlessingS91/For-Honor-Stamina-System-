#include "MultiHittingBalance.h"

#include <chrono>
#include <cmath>
#include <cstdint>
#include <mutex>
#include <unordered_map>
#include <utility>
#include <vector>

#include "Settings.h"

namespace MultiHittingBalance {

    namespace {

        using Clock = std::chrono::steady_clock;

        // Fail-safe for missed animation reset events. This only resets a
        // swing sequence after an actor has stopped swinging for 3 seconds.
        constexpr auto kSwingInactivityTimeout = std::chrono::seconds(3);

        // Native ActorHandle IDs identify an actor without retaining a raw RE::Actor*.
        // The handle also allows expired/unloaded actors to be pruned safely.
        struct TrackedSwingState {
            RE::ActorHandle handle;
            SwingState state;
            Clock::time_point lastSwingAt{};
        };

        std::unordered_map<std::uint32_t, TrackedSwingState> swingStates;
        std::mutex swingStatesMutex;

        // A rare, amortized cleanup on actual swings; no work added to Actor::Update.
        constexpr std::uint32_t kPruneEverySwings = 128;
        std::uint32_t swingsSincePrune = 0;

        constexpr float kFirstSwingMultiplier = 1.0f;

        // Use the actor's CURRENT race rather than its NPC base record.
        // This excludes transformed players (e.g. werewolves) as well as creatures.
        bool IsHumanoid(RE::Actor* actor) {
            if (!actor) {
                return false;
            }

            static auto* actorTypeNPC = RE::TESForm::LookupByID<RE::BGSKeyword>(0x00013794);
            auto* race = actor->GetRace();
            return race && actorTypeNPC && race->HasKeyword(actorTypeNPC);
        }

        float GetMultiplier(std::uint32_t swingCount) {
            if (swingCount <= 1) {
                return kFirstSwingMultiplier;
            }

            return std::pow(Settings::multiHitting.scalingMultiplier, static_cast<float>(swingCount - 1));
        }

        // Lookups never create a new entry. Before an actor's first swing, the
        // default SwingState produces the original neutral 1.0x multiplier.
        SwingState GetStateSnapshot(RE::Actor* actor) {
            // Non-humanoids have no multi-hit stacks, even if this same actor
            // was previously tracked before changing race.
            if (!IsHumanoid(actor)) {
                return {};
            }

            const auto handle = actor->GetHandle();
            if (!handle) {
                return {};
            }

            const auto now = Clock::now();
            std::lock_guard lock(swingStatesMutex);
            const auto it = swingStates.find(handle.native_handle());
            if (it == swingStates.end()) {
                return {};
            }

            // Timed-out stacks must not affect damage/stamina even if no new
            // weaponSwing event has arrived yet.
            if (now - it->second.lastSwingAt >= kSwingInactivityTimeout) {
                swingStates.erase(it);
                return {};
            }

            return it->second.state;
        }

        void PruneExpiredHandles() {
            // Resolve handles outside our mutex: handle.get() can call engine code.
            std::vector<std::pair<std::uint32_t, RE::ActorHandle>> snapshot;
            {
                std::lock_guard lock(swingStatesMutex);
                snapshot.reserve(swingStates.size());
                for (const auto& [key, tracked] : swingStates) {
                    snapshot.emplace_back(key, tracked.handle);
                }
            }

            for (const auto& [key, handle] : snapshot) {
                if (handle.get()) {
                    continue;
                }

                std::lock_guard lock(swingStatesMutex);
                const auto it = swingStates.find(key);
                if (it != swingStates.end() && it->second.handle == handle) {
                    swingStates.erase(it);
                }
            }
        }

        std::uint32_t RegisterSwing(RE::Actor* actor, bool leftHand) {
            if (!actor) {
                return 0;
            }

            if (!IsHumanoid(actor)) {
                // Discard counters from before a transformation rather than
                // allowing them to reappear after returning to humanoid form.
                Reset(actor);
                return 0;
            }

            const auto handle = actor->GetHandle();
            if (!handle) {
                return 0;
            }

            const auto now = Clock::now();
            std::uint32_t count = 0;
            bool shouldPrune = false;
            {
                std::lock_guard lock(swingStatesMutex);
                auto it = swingStates.try_emplace(
                    handle.native_handle(), TrackedSwingState{handle, {}}).first;
                auto& tracked = it->second;

                // A missed combo-end/block/stagger event cannot leave old
                // stacks attached to the next, unrelated attack sequence.
                if (now - tracked.lastSwingAt >= kSwingInactivityTimeout) {
                    tracked.state = {};
                }
                tracked.lastSwingAt = now;
                auto& state = tracked.state;

                if (leftHand) {
                    count = ++state.leftHandSwings;
                } else {
                    count = ++state.rightHandSwings;
                }
                state.lastSwingLeft = leftHand;

                shouldPrune = ++swingsSincePrune >= kPruneEverySwings;
                if (shouldPrune) {
                    swingsSincePrune = 0;
                }
            }

            if (shouldPrune) {
                PruneExpiredHandles();
            }
            return count;
        }

    }

    void Reset(RE::Actor* actor) {
        if (!actor) {
            return;
        }

        const auto handle = actor->GetHandle();
        if (!handle) {
            return;
        }

        // An erased entry is equivalent to the old zeroed state, but won't
        // accumulate forever for actors that have finished attacking.
        std::lock_guard lock(swingStatesMutex);
        swingStates.erase(handle.native_handle());
    }

    void ResetAll() {
        std::lock_guard lock(swingStatesMutex);
        swingStates.clear();
        swingsSincePrune = 0;
    }

    void RegisterRightSwing(RE::Actor* actor) {
        if (!actor || !Settings::multiHitEnabled) {
            return;
        }

        const auto count = RegisterSwing(actor, false);
        if (Settings::debugLogging && count > 0) {
            logger::info("[MultiHit] {} RIGHT swing #{} | multiplier={:.4f}", actor->GetName(), count,
                         GetMultiplier(count));
        }
    }

    void RegisterLeftSwing(RE::Actor* actor) {
        if (!actor || !Settings::multiHitEnabled) {
            return;
        }

        const auto count = RegisterSwing(actor, true);
        if (Settings::debugLogging && count > 0) {
            logger::info("[MultiHit] {} LEFT swing #{} | multiplier={:.4f}", actor->GetName(), count,
                         GetMultiplier(count));
        }
    }

    std::uint32_t GetRightSwingCount(RE::Actor* actor) {
        return GetStateSnapshot(actor).rightHandSwings;
    }

    std::uint32_t GetLeftSwingCount(RE::Actor* actor) {
        return GetStateSnapshot(actor).leftHandSwings;
    }

    float GetStaminaCostMultiplier(RE::Actor* actor, bool leftHand) {
        if (!actor || !Settings::multiHitEnabled) {
            return 1.0f;
        }

        const auto state = GetStateSnapshot(actor);
        const auto swingCount = leftHand ? state.leftHandSwings : state.rightHandSwings;
        const auto multiplier = GetMultiplier(swingCount);

        if (Settings::debugLogging) {
            logger::info("[MultiHit] {} {} stamina | swing #{} | multiplier={:.4f}", actor->GetName(),
                         leftHand ? "LEFT" : "RIGHT", swingCount, multiplier);
        }

        return multiplier;
    }

    float GetDamageMultiplier(RE::Actor* actor, bool leftHand) {
        if (!actor || !Settings::multiHitEnabled) {
            return 1.0f;
        }

        const auto state = GetStateSnapshot(actor);
        const auto swingCount = leftHand ? state.leftHandSwings : state.rightHandSwings;
        return GetMultiplier(swingCount);
    }

    float GetCurrentDamageMultiplier(RE::Actor* actor) {
        if (!actor || !Settings::multiHitEnabled) {
            return 1.0f;
        }

        const auto state = GetStateSnapshot(actor);
        const auto swingCount = state.lastSwingLeft ? state.leftHandSwings : state.rightHandSwings;
        return GetMultiplier(swingCount);
    }

    float ApplyDamageMultiplier(RE::Actor* actor, float damage, bool leftHand) {
        if (!actor || !Settings::multiHitEnabled) {
            return damage;
        }

        return damage * GetDamageMultiplier(actor, leftHand);
    }

}
