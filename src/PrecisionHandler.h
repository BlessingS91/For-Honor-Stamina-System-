#pragma once

#include <Includes/PrecisionAPI.h>

namespace PrecisionHandler {

    void Install();

    PRECISION_API::PreHitCallbackReturn ProcessPreHit(const PRECISION_API::PrecisionHitData& hitData);

}