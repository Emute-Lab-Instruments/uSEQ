//
// Created by ck84 on 01/10/23.
//

#ifndef VIRTUAL_STRING_H
#define VIRTUAL_STRING_H

#ifndef STRING_H_
#define STRING_H_

#include <sstream>
#include <string>

class String : public std::string {
public:
    String(const std::string &str) : std::string(str) {}
    String() : std::string("") {}

    // Constructor taking const double&
    String(const double &val) {
        std::ostringstream oss;
        oss << val;
        *this = oss.str();
    }

    void replace(const std::string &from, const std::string &to) {
        if (from.empty()) {
            return;
        }

        std::string newStr;
        size_t pos = 0;
        size_t lastPos = 0;

        while ((pos = this->find(from, lastPos)) != std::string::npos) {
            newStr.append(*this, lastPos, pos - lastPos);
            newStr.append(to);
            lastPos = pos + from.length();
        }

        // Append any remaining characters from the original string
        newStr.append(*this, lastPos, this->length() - lastPos);

        // Assign the newly created string back to *this
        *this = std::move(newStr);
    }

    int indexOf(char c) const {
        size_t pos = this->find(c);
        if (pos != std::string::npos) {
            return static_cast<int>(pos);
        }
        return -1;
    }

    String substring(size_t pos = 0, size_t len = std::string::npos) const {
        String result = std::string::substr(pos, len);
        return result;
    }

    char charAt(unsigned int index) {
        return (*this)[index];
    }

    void setCharAt(unsigned int index, char c) {
        this[index] = c;
    }

    // Dispatch all other constructors to std::string
    using std::string::string;
};

#endif // STRING_H_
#endif //VIRTUAL_STRING_H
