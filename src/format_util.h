#pragma once

#include <Arduino.h>

// 1740.73 with 2 decimals -> "1,740.73" (English thousands separators).
String fmtGrouped(double value, int decimals);

// "$76,032" (>= 1000), "$692.97" (>= 1), "$0.6234" (< 1).
String coinPriceText(double usd);

// VPS prices are in thousand VND: 57.5 -> "57,500".
String stockPriceText(double thousandVnd);

// 0.44 -> "+0.44%", -5.11 -> "-5.11%".
String signedPercent(double value);

// "Updated" stamp: "HH:MM" when the data is from today, "dd/mm HH:MM" otherwise.
String updatedText(const char *date, const char *time, const String &today);
