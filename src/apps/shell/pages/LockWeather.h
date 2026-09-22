#pragma once

#include <cstdint>

#include "../icons/WeatherDotIcons.h"

namespace apps::shell::pages {

// WWO condition codes grouped into six icon categories; detailed labels stay independent.
inline platform::ui::DotMatrix weatherIcon(uint16_t code) {
    switch (code) {
        case 113:
            return icons::Clear;
        case 116:
            return icons::PartlyCloudy;
        case 119:
        case 122:
        case 143:
        case 248:
        case 260:
            return icons::Cloudy;
        case 176:
        case 263:
        case 266:
        case 293:
        case 296:
        case 353:
        case 299:
        case 302:
        case 305:
        case 308:
        case 356:
        case 359:
            return icons::Rain;
        case 200:
        case 386:
        case 389:
        case 392:
        case 395:
            return icons::Thunderstorm;
        case 179:
        case 227:
        case 230:
        case 323:
        case 326:
        case 329:
        case 332:
        case 335:
        case 338:
        case 368:
        case 371:
            return icons::Snow;
        case 182:
        case 317:
        case 320:
        case 362:
        case 365:
            return icons::Snow;
        case 185:
        case 281:
        case 284:
        case 311:
        case 314:
        case 350:
        case 374:
        case 377:
            return icons::Snow;
        default:
            return {};
    }
}

// Compact WWO labels for the half-width lock-screen card.
inline const char* weatherCondition(uint16_t code) {
    switch (code) {
        case 113:
            return "Sunny";
        case 116:
            return "Partly cloudy";
        case 119:
            return "Cloudy";
        case 122:
            return "Overcast";
        case 143:
            return "Mist";
        case 176:
            return "Patchy rain";
        case 179:
            return "Patchy snow";
        case 182:
            return "Patchy sleet";
        case 185:
        case 281:
        case 284:
            return "Icy drizzle";
        case 200:
            return "Thunder";
        case 227:
            return "Blowing snow";
        case 230:
            return "Blizzard";
        case 248:
            return "Fog";
        case 260:
            return "Freezing fog";
        case 263:
        case 266:
            return "Drizzle";
        case 293:
        case 296:
            return "Light rain";
        case 299:
        case 302:
            return "Rain";
        case 305:
        case 308:
            return "Heavy rain";
        case 311:
        case 314:
            return "Freezing rain";
        case 317:
        case 320:
            return "Sleet";
        case 323:
        case 326:
            return "Light snow";
        case 329:
        case 332:
            return "Snow";
        case 335:
        case 338:
            return "Heavy snow";
        case 350:
        case 374:
        case 377:
            return "Ice pellets";
        case 353:
        case 356:
        case 359:
            return "Rain showers";
        case 362:
        case 365:
            return "Sleet showers";
        case 368:
        case 371:
            return "Snow showers";
        case 386:
        case 389:
            return "Thunderstorm";
        case 392:
        case 395:
            return "Thundersnow";
        default:
            return "--";
    }
}

}  // namespace apps::shell::pages
