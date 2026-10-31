#include "mk/types.hpp"

#include <algorithm>
#include <cstdint>
#include <numeric>
#include <string>

namespace mk {
namespace {

using u64 = uint64_t;

u64 magnitude(int64_t value) {
    if (value >= 0) {
        return static_cast<u64>(value);
    }
    return static_cast<u64>(-(value + 1)) + 1U;
}

int64_t checked_add(int64_t a, int64_t b) {
    if ((b > 0 && a > INT64_MAX - b) ||
        (b < 0 && a < INT64_MIN - b)) {
        throw CalcError("integer overflow");
    }
    return a + b;
}

int64_t checked_sub(int64_t a, int64_t b) {
    if ((b < 0 && a > INT64_MAX + b) ||
        (b > 0 && a < INT64_MIN + b)) {
        throw CalcError("integer overflow");
    }
    return a - b;
}

int64_t checked_mul(int64_t a, int64_t b) {
    if (a == 0 || b == 0) {
        return 0;
    }
    if (a == -1 && b == INT64_MIN) {
        throw CalcError("integer overflow");
    }
    if (b == -1 && a == INT64_MIN) {
        throw CalcError("integer overflow");
    }

    const u64 aa = magnitude(a);
    const u64 bb = magnitude(b);
    const u64 limit = ((a < 0) != (b < 0))
        ? (u64{1} << 63)
        : static_cast<u64>(INT64_MAX);

    if (aa > limit / bb) {
        throw CalcError("integer overflow");
    }

    const u64 result = aa * bb;
    if ((a < 0) != (b < 0)) {
        if (result == (u64{1} << 63)) {
            return INT64_MIN;
        }
        return -static_cast<int64_t>(result);
    }
    return static_cast<int64_t>(result);
}

int64_t checked_negate(int64_t value) {
    if (value == INT64_MIN) {
        throw CalcError("integer overflow");
    }
    return -value;
}

int64_t unsigned_to_signed(u64 value) {
    if (value > static_cast<u64>(INT64_MAX)) {
        throw CalcError("integer overflow");
    }
    return static_cast<int64_t>(value);
}

Rational make_rational(int64_t numerator, int64_t denominator) {
    return Rational(numerator, denominator);
}

}

CalcError::CalcError(const char* message) : std::runtime_error(message) {}
CalcError::CalcError(const std::string& message) : std::runtime_error(message) {}

Money& Money::operator+=(Money other) {
    cents = checked_add(cents, other.cents);
    return *this;
}

Money& Money::operator-=(Money other) {
    cents = checked_sub(cents, other.cents);
    return *this;
}

Money operator+(Money left, Money right) {
    left += right;
    return left;
}

Money operator-(Money left, Money right) {
    left -= right;
    return left;
}

bool operator==(Money left, Money right) { return left.cents == right.cents; }
bool operator!=(Money left, Money right) { return !(left == right); }
bool operator<(Money left, Money right) { return left.cents < right.cents; }
bool operator<=(Money left, Money right) { return !(right < left); }
bool operator>(Money left, Money right) { return right < left; }
bool operator>=(Money left, Money right) { return !(left < right); }

Rational::Rational() : numerator_(0), denominator_(1) {}
Rational::Rational(int64_t value) : numerator_(value), denominator_(1) {}
Rational::Rational(Money value) : numerator_(value.cents), denominator_(1) {}

Rational::Rational(int64_t numerator, int64_t denominator)
    : numerator_(numerator), denominator_(denominator) {
    if (denominator == 0) {
        throw CalcError("division by zero");
    }
    if (numerator == 0) {
        denominator_ = 1;
        return;
    }
    if (denominator == INT64_MIN) {
        throw CalcError("denominator cannot be represented as positive");
    }
    if (denominator < 0) {
        numerator_ = checked_negate(numerator);
        denominator_ = -denominator;
    }

    const u64 divisor = std::gcd(magnitude(numerator_), magnitude(denominator_));
    numerator_ = static_cast<int64_t>(
        numerator_ < 0
            ? -static_cast<int64_t>(magnitude(numerator_) / divisor)
            : static_cast<int64_t>(magnitude(numerator_) / divisor));
    denominator_ = static_cast<int64_t>(
        magnitude(denominator_) / divisor);
}

int64_t Rational::numerator() const noexcept { return numerator_; }
int64_t Rational::denominator() const noexcept { return denominator_; }

Rational Rational::operator-() const {
    return Rational(checked_negate(numerator_), denominator_);
}

Rational& Rational::operator+=(const Rational& other) {
    *this = *this + other;
    return *this;
}

Rational& Rational::operator-=(const Rational& other) {
    *this = *this - other;
    return *this;
}

Rational& Rational::operator*=(const Rational& other) {
    *this = *this * other;
    return *this;
}

Rational& Rational::operator/=(const Rational& other) {
    *this = *this / other;
    return *this;
}

Rational operator+(Rational left, const Rational& right) {
    const u64 divisor = std::gcd(magnitude(left.denominator_), magnitude(right.denominator_));
    const int64_t left_factor = left.denominator_ / static_cast<int64_t>(divisor);
    const int64_t right_factor = right.denominator_ / static_cast<int64_t>(divisor);
    const int64_t first = checked_mul(left.numerator_, right_factor);
    const int64_t second = checked_mul(right.numerator_, left_factor);
    const int64_t numerator = checked_add(first, second);
    const int64_t denominator = checked_mul(left_factor, right.denominator_);
    return make_rational(numerator, denominator);
}

Rational operator-(Rational left, const Rational& right) {
    return left + (-right);
}

Rational operator*(Rational left, const Rational& right) {
    const u64 g1 = std::gcd(magnitude(left.numerator_), magnitude(right.denominator_));
    const u64 g2 = std::gcd(magnitude(right.numerator_), magnitude(left.denominator_));
    const int64_t a = left.numerator_ / static_cast<int64_t>(g1);
    const int64_t b = right.numerator_ / static_cast<int64_t>(g2);
    const int64_t c = left.denominator_ / static_cast<int64_t>(g2);
    const int64_t d = right.denominator_ / static_cast<int64_t>(g1);
    return make_rational(checked_mul(a, b), checked_mul(c, d));
}

Rational operator/(Rational left, const Rational& right) {
    if (right.numerator_ == 0) {
        throw CalcError("division by zero");
    }
    return left * Rational(right.denominator_, right.numerator_);
}

bool operator==(const Rational& left, const Rational& right) {
    return left.numerator_ == right.numerator_ &&
           left.denominator_ == right.denominator_;
}

bool operator!=(const Rational& left, const Rational& right) { return !(left == right); }

bool operator<(const Rational& left, const Rational& right) {
    const u64 g = std::gcd(magnitude(left.denominator_), magnitude(right.denominator_));
    const int64_t lf = left.denominator_ / static_cast<int64_t>(g);
    const int64_t rf = right.denominator_ / static_cast<int64_t>(g);
    const int64_t a = checked_mul(left.numerator_, rf);
    const int64_t b = checked_mul(right.numerator_, lf);
    return a < b;
}

bool operator<=(const Rational& left, const Rational& right) { return !(right < left); }
bool operator>(const Rational& left, const Rational& right) { return right < left; }
bool operator>=(const Rational& left, const Rational& right) { return !(left < right); }

int64_t Rational::round_half_up_to_int64() const {
    const int64_t quotient = numerator_ / denominator_;
    const int64_t remainder = numerator_ % denominator_;
    const u64 twice_remainder = magnitude(remainder) > (UINT64_MAX / 2U)
        ? UINT64_MAX
        : magnitude(remainder) * 2U;

    if (twice_remainder < static_cast<u64>(denominator_)) {
        return quotient;
    }
    return remainder < 0
        ? checked_sub(quotient, 1)
        : checked_add(quotient, 1);
}

namespace {

bool is_leap(int32_t year) {
    return year % 4 == 0 && (year % 100 != 0 || year % 400 == 0);
}

int64_t days_from_civil(int32_t year, unsigned month, unsigned day) {
    year -= month <= 2;
    const int32_t era = (year >= 0 ? year : year - 399) / 400;
    const unsigned year_of_era = static_cast<unsigned>(year - era * 400);
    const unsigned day_of_year =
        (153U * (month + (month > 2 ? static_cast<unsigned>(-3) : 9U)) + 2U) / 5U
        + day - 1U;
    const unsigned day_of_era =
        year_of_era * 365U + year_of_era / 4U - year_of_era / 100U + day_of_year;
    return static_cast<int64_t>(era) * 146097 + static_cast<int64_t>(day_of_era) - 719468;
}

void civil_from_days(int64_t ordinal, int32_t& year, int32_t& month, int32_t& day) {
    ordinal += 719468;
    const int64_t era = (ordinal >= 0 ? ordinal : ordinal - 146096) / 146097;
    const unsigned day_of_era = static_cast<unsigned>(ordinal - era * 146097);
    const unsigned year_of_era =
        (day_of_era - day_of_era / 1460U + day_of_era / 36524U -
         day_of_era / 146096U) / 365U;
    const int32_t year_value =
        static_cast<int32_t>(year_of_era) + static_cast<int32_t>(era) * 400;
    const unsigned day_of_year =
        day_of_era - (365U * year_of_era + year_of_era / 4U -
                      year_of_era / 100U);
    const unsigned month_value = (5U * day_of_year + 2U) / 153U;
    day = static_cast<int32_t>(
        day_of_year - (153U * month_value + 2U) / 5U + 1U);
    month = static_cast<int32_t>(
        month_value + (month_value < 10U ? 3U : static_cast<unsigned>(-9)));
    year = year_value + (month <= 2 ? 1 : 0);
}

}

Date::Date(int32_t year, int32_t month, int32_t day)
    : ordinal_(0) {
    if (year < 1 || year > 9999 || month < 1 || month > 12 ||
        day < 1 || day > days_in_month(year, month)) {
        throw CalcError("invalid date");
    }
    ordinal_ = static_cast<int32_t>(
        days_from_civil(year, static_cast<unsigned>(month), static_cast<unsigned>(day)));
}

Date::Date(int32_t ordinal, bool) : ordinal_(ordinal) {}

Date Date::from_ordinal(int32_t ordinal) {
    const int64_t minimum = days_from_civil(1, 1, 1);
    const int64_t maximum = days_from_civil(9999, 12, 31);
    if (ordinal < minimum || ordinal > maximum) {
        throw CalcError("date ordinal out of range");
    }
    return Date(ordinal, true);
}

int32_t Date::year() const {
    int32_t result_year;
    int32_t result_month;
    int32_t result_day;
    civil_from_days(ordinal_, result_year, result_month, result_day);
    return result_year;
}

int32_t Date::month() const {
    int32_t result_year;
    int32_t result_month;
    int32_t result_day;
    civil_from_days(ordinal_, result_year, result_month, result_day);
    return result_month;
}

int32_t Date::day() const {
    int32_t result_year;
    int32_t result_month;
    int32_t result_day;
    civil_from_days(ordinal_, result_year, result_month, result_day);
    return result_day;
}

int32_t Date::ordinal() const noexcept { return ordinal_; }

int32_t Date::days_in_month(int32_t year, int32_t month) {
    if (year < 1 || year > 9999 || month < 1 || month > 12) {
        throw CalcError("invalid month");
    }
    static const int32_t days[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (month == 2 && is_leap(year)) {
        return 29;
    }
    return days[month - 1];
}

Date Date::first_of_month(int32_t year, int32_t month) {
    return Date(year, month, 1);
}

Date operator+(Date date, int32_t days) {
    const int64_t result = static_cast<int64_t>(date.ordinal_) + days;
    return Date::from_ordinal(static_cast<int32_t>(result));
}

int32_t operator-(Date left, Date right) {
    const int64_t result =
        static_cast<int64_t>(left.ordinal_) - static_cast<int64_t>(right.ordinal_);
    return static_cast<int32_t>(result);
}

bool operator==(Date left, Date right) { return left.ordinal_ == right.ordinal_; }
bool operator!=(Date left, Date right) { return !(left == right); }
bool operator<(Date left, Date right) { return left.ordinal_ < right.ordinal_; }
bool operator<=(Date left, Date right) { return !(right < left); }
bool operator>(Date left, Date right) { return right < left; }
bool operator>=(Date left, Date right) { return !(left < right); }

}
