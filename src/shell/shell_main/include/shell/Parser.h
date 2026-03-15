#pragma once

// #include <stdint.h>
// #include <ctype.h>
// #include <string.h>

enum class ParseError {
    ERROR
};

template<typename R, typename E>
struct Result {
    Result(E e) {
        this->err = true;
        this->r = R();
        this->e = e;
    }

    Result(R r) {
        this->err = false;
        this->r = r;
        this->e = E();
    }

    R ok_or(R default_val) {
        return err ? default_val : r;
    }

    bool is_ok() const { return !err; }

    bool is_err() const { return err; }

    operator R() const {
        return r;
    }

    operator E() const {
        return e;
    }

    static Result Ok(R r) {
        Result result;
        result.err = false;
        result.r = r;
        result.e = E();
        return result;
    }

    static Result Err(E e) {
        Result result;
        result.err = true;
        result.r = R();
        result.e = e;
        return result;
    }

    bool err;
    R r;
    E e;
};


Result<int, ParseError> take_int(const char *s);
