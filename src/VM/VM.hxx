#pragma once

#include <string>
#include <vector>
#include <unordered_map>


#include "../Value/Value.hxx"
#include "ByteCode.hxx"


inline namespace pie {
namespace vm {

using ID = ssize_t;
struct Frame {
    size_t bp, ret_address;

    std::unordered_map<ID, value::Value> env;
};

class Machine {

public:
    std::unordered_map<ssize_t, const value::Value> constants;

    std::vector<value::Value> stack;

    std::vector<Frame> frames;
    std::unordered_map<std::string, size_t> labels;
    std::vector<Code> instructions;


    auto& currentEnv() { return frames.back().env; }


    void execute() {
        using enum Code;

        for (size_t ip{}; ; ++ip) {
            const auto instruction = instructions[ip];

            switch (instruction) {
                case POP: stack.pop_back();
                    break;

                case LOAD_CONST: {
                    const auto input = static_cast<ID>(instructions[++ip]);
                    const auto& value = constants.at(input);
                    stack.push_back(value);
                    break;
                } break;

                case LOAD: {
                    const auto input = static_cast<ID>(instructions[ip]);
                    const auto& value = currentEnv().at(input);
                    stack.push_back(value);
                    break;
                } break;

                case STORE: {
                    const auto input = static_cast<ID>(instructions[++ip]);
                    const auto& value = currentEnv().at(input);
                    stack.push_back(value);
                } break;


                case LOAD_ACC :
                case STORE_ACC:

                case CALL:


                case RET:


                case LT     :
                case LEQ    :
                case EQ     :
                case GEQ    :
                case GT     :
                case JUMP_IF:
                case JUMP   :



                case HALT:
            }
        }
    }

};

} // namespace vm
} // namespace pie