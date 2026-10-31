#ifndef MK_TYPES_HPP
#define MK_TYPES_HPP

#include <cstdint>
#include <stdexcept>

namespace mk {

class CalcError : public std::runtime_error {
public:
    explicit CalcError(const char* message);
    explicit CalcError(const std::string& message);
};

class Money {
public:
    int64_t cents = 0;

    Money() = default;
    explicit Money(int64_t value) : cents(value) {}

    Money& operator+=(Money other);
    Money& operator-=(Money other);

    friend Money operator+(Money left, Money right);
    friend Money operator-(Money left, Money right);

    friend bool operator==(Money left, Money right);
    friend bool operator!=(Money left, Money right);
    friend bool operator<(Money left, Money right);
    friend bool operator<=(Money left, Money right);
    friend bool operator>(Money left, Money right);
    friend bool operator>=(Money left, Money right);
};

class Rational {
public:
    Rational();
    explicit Rational(int64_t value);
    explicit Rational(Money value);
    Rational(int64_t numerator, int64_t denominator);

    int64_t numerator() const noexcept;
    int64_t denominator() const noexcept;

    Rational operator-() const;
    Rational& operator+=(const Rational& other);
    Rational& operator-=(const Rational& other);
    Rational& operator*=(const Rational& other);
    Rational& operator/=(const Rational& other);

    int64_t round_half_up_to_int64() const;

    friend Rational operator+(Rational left, const Rational& right);
    friend Rational operator-(Rational left, const Rational& right);
    friend Rational operator*(Rational left, const Rational& right);
    friend Rational operator/(Rational left, const Rational& right);

    friend bool operator==(const Rational& left, const Rational& right);
    friend bool operator!=(const Rational& left, const Rational& right);
    friend bool operator<(const Rational& left, const Rational& right);
    friend bool operator<=(const Rational& left, const Rational& right);
    friend bool operator>(const Rational& left, const Rational& right);
    friend bool operator>=(const Rational& left, const Rational& right);

private:
    int64_t numerator_;
    int64_t denominator_;
};

class Date {
public:
    Date(int32_t year, int32_t month, int32_t day);
    static Date from_ordinal(int32_t ordinal);

    int32_t year() const;
    int32_t month() const;
    int32_t day() const;
    int32_t ordinal() const noexcept;

    static int32_t days_in_month(int32_t year, int32_t month);
    static Date first_of_month(int32_t year, int32_t month);

    friend Date operator+(Date date, int32_t days);
    friend int32_t operator-(Date left, Date right);

    friend bool operator==(Date left, Date right);
    friend bool operator!=(Date left, Date right);
    friend bool operator<(Date left, Date right);
    friend bool operator<=(Date left, Date right);
    friend bool operator>(Date left, Date right);
    friend bool operator>=(Date left, Date right);

private:
    explicit Date(int32_t ordinal, bool);
    int32_t ordinal_;
};

}

#endif
