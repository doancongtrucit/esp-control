#include "format_util.h"

#include <math.h>

String fmtGrouped(double value, int decimals) {
  char buffer[32];
  snprintf(buffer, sizeof(buffer), "%.*f", decimals, fabs(value));
  String digits(buffer);
  const int dot = digits.indexOf('.');
  String whole = dot >= 0 ? digits.substring(0, dot) : digits;
  const String fraction = dot >= 0 ? digits.substring(dot) : String();
  String grouped;
  int count = 0;
  for (int i = whole.length() - 1; i >= 0; --i) {
    grouped = whole[i] + grouped;
    if (++count % 3 == 0 && i > 0) grouped = "," + grouped;
  }
  // Signed only when the rounded text is not all zeros (-0.004 -> "0.00").
  bool nonZero = false;
  for (const char *c = buffer; *c; ++c) nonZero |= (*c >= '1' && *c <= '9');
  const bool negative = value < 0 && nonZero;
  return (negative ? "-" : "") + grouped + fraction;
}

String coinPriceText(double usd) {
  if (usd >= 1000) return "$" + fmtGrouped(usd, 0);
  if (usd >= 1) return "$" + fmtGrouped(usd, 2);
  return "$" + fmtGrouped(usd, 4);
}

String stockPriceText(double thousandVnd) { return fmtGrouped(round(thousandVnd * 1000), 0); }

String signedPercent(double value) {
  return (value > 0 ? "+" : "") + fmtGrouped(value, 2) + "%";
}
