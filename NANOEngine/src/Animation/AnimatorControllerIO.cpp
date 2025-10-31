#include "AnimatorControllerIO.hpp"
#include <rapidjson/document.h>
#include <rapidjson/writer.h>
#include <rapidjson/stringbuffer.h>
#include <fstream>

using namespace rapidjson;

namespace NE::Animation {

    static const char* ParamTypeStr(ParamType t) {
        switch (t) {
        case ParamType::Bool: return "Bool";
        case ParamType::Float:return "Float";
        case ParamType::Int:  return "Int";
        case ParamType::Trigger:return "Trigger";
        }
        return "Bool";
    }
    static ParamType ParseParamType(const std::string& s) {
        if (s == "Bool") return ParamType::Bool;
        if (s == "Float")return ParamType::Float;
        if (s == "Int")  return ParamType::Int;
        if (s == "Trigger") return ParamType::Trigger;
        return ParamType::Bool;
    }
    static const char* OpStr(CondOp o) {
        switch (o) {
        case CondOp::If: return "If";
        case CondOp::IfNot: return "IfNot";
        case CondOp::Greater: return ">";
        case CondOp::Less: return "<";
        case CondOp::Equals: return "==";
        case CondOp::NotEquals: return "!=";
        }
        return "If";
    }
    static CondOp ParseOp(const std::string& s) {
        if (s == "If") return CondOp::If;
        if (s == "IfNot") return CondOp::IfNot;
        if (s == ">") return CondOp::Greater;
        if (s == "<") return CondOp::Less;
        if (s == "==") return CondOp::Equals;
        if (s == "!=") return CondOp::NotEquals;
        return CondOp::If;
    }

    bool SaveAnimatorController(const AnimatorController& c, const std::string& path) {
        Document d; d.SetObject(); auto& a = d.GetAllocator();
        d.AddMember("name", Value(c.name.c_str(), a), a);
        d.AddMember("defaultState", c.defaultState, a);

        Value params(kArrayType);
        for (auto& p : c.parameters) {
            Value jp(kObjectType);
            jp.AddMember("name", Value(p.name.c_str(), a), a);
            jp.AddMember("type", Value(ParamTypeStr(p.type), a), a);
            switch (p.type) {
            case ParamType::Bool:   jp.AddMember("default", p.b, a); break;
            case ParamType::Float:  jp.AddMember("default", p.f, a); break;
            case ParamType::Int:    jp.AddMember("default", p.i, a); break;
            case ParamType::Trigger:jp.AddMember("default", false, a); break;
            }
            params.PushBack(jp, a);
        }
        d.AddMember("parameters", params, a);

        Value states(kArrayType);
        for (auto& s : c.states) {
            Value js(kObjectType);
            js.AddMember("name", Value(s.name.c_str(), a), a);
            js.AddMember("clipId", Value(s.clipId.c_str(), a), a);
            js.AddMember("speed", s.speed, a);

            Value trans(kArrayType);
            for (auto& t : s.transitions) {
                Value jt(kObjectType);
                jt.AddMember("to", t.toState, a);
                jt.AddMember("hasExitTime", t.hasExitTime, a);
                jt.AddMember("exitTime", t.exitTimeNormalized, a);
                jt.AddMember("duration", t.duration, a);

                Value conds(kArrayType);
                for (auto& cnd : t.conditions) {
                    Value jc(kObjectType);
                    jc.AddMember("param", Value(cnd.param.c_str(), a), a);
                    jc.AddMember("op", Value(OpStr(cnd.op), a), a);
                    jc.AddMember("b", cnd.b, a);
                    jc.AddMember("f", cnd.f, a);
                    jc.AddMember("i", cnd.i, a);
                    conds.PushBack(jc, a);
                }
                jt.AddMember("conditions", conds, a);
                trans.PushBack(jt, a);
            }
            js.AddMember("transitions", trans, a);
            states.PushBack(js, a);
        }
        d.AddMember("states", states, a);

        StringBuffer buf; Writer<StringBuffer> wr(buf); d.Accept(wr);
        std::ofstream out(path); if (!out) return false;
        out << buf.GetString();
        return true;
    }

    bool LoadAnimatorController(AnimatorController& c, const std::string& path) {
        std::ifstream in(path, std::ios::binary);
        if (!in.good()) return false;

        std::string s((std::istreambuf_iterator<char>(in)), {});
        rapidjson::Document d;
        if (d.Parse(s.c_str()).HasParseError() || !d.IsObject())
            return false;

        // name
        if (!d.HasMember("name") || !d["name"].IsString()) return false;
        c.name = d["name"].GetString();

        // defaultState
        if (!d.HasMember("defaultState") || !d["defaultState"].IsUint()) return false;
        c.defaultState = d["defaultState"].GetUint();

        // parameters (optional -> treat as empty if missing/wrong type)
        c.parameters.clear();
        if (d.HasMember("parameters") && d["parameters"].IsArray()) {
            for (const auto& jp : d["parameters"].GetArray()) {
                if (!jp.IsObject() || !jp.HasMember("name") || !jp["name"].IsString() ||
                    !jp.HasMember("type") || !jp["type"].IsString())
                    continue;
                Parameter p;
                p.name = jp["name"].GetString();
                p.type = ParseParamType(jp["type"].GetString());
                if (p.type == ParamType::Bool && jp.HasMember("default") && jp["default"].IsBool())   p.b = jp["default"].GetBool();
                if (p.type == ParamType::Float && jp.HasMember("default") && jp["default"].IsFloat())  p.f = jp["default"].GetFloat();
                if (p.type == ParamType::Int && jp.HasMember("default") && jp["default"].IsInt())    p.i = jp["default"].GetInt();
                c.parameters.push_back(p);
            }
        }

        // states (required)
        if (!d.HasMember("states") || !d["states"].IsArray()) return false;
        c.states.clear();
        for (const auto& js : d["states"].GetArray()) {
            if (!js.IsObject() || !js.HasMember("clipId") || !js["clipId"].IsString())
                continue;

            State st;
            st.name = (js.HasMember("name") && js["name"].IsString()) ? js["name"].GetString() : "";
            st.clipId = js["clipId"].GetString();
            st.speed = (js.HasMember("speed") && js["speed"].IsNumber()) ? js["speed"].GetFloat() : 1.0f;

            if (js.HasMember("transitions") && js["transitions"].IsArray()) {
                for (const auto& jt : js["transitions"].GetArray()) {
                    if (!jt.IsObject() || !jt.HasMember("to") || !jt["to"].IsUint()) continue;
                    Transition tr;
                    tr.toState = jt["to"].GetUint();
                    tr.hasExitTime = jt.HasMember("hasExitTime") && jt["hasExitTime"].IsBool() ? jt["hasExitTime"].GetBool() : false;
                    tr.exitTimeNormalized = jt.HasMember("exitTime") && jt["exitTime"].IsNumber() ? jt["exitTime"].GetFloat() : 0.0f;
                    tr.duration = jt.HasMember("duration") && jt["duration"].IsNumber() ? jt["duration"].GetFloat() : 0.2f;
                    tr.canTransitionToSelf = jt.HasMember("canTransitionToSelf") && jt["canTransitionToSelf"].IsBool() ? jt["canTransitionToSelf"].GetBool() : false;

                    if (jt.HasMember("conditions") && jt["conditions"].IsArray()) {
                        for (const auto& jc : jt["conditions"].GetArray()) {
                            if (!jc.IsObject() || !jc.HasMember("param") || !jc["param"].IsString() || !jc.HasMember("op") || !jc["op"].IsString())
                                continue;
                            Condition cd;
                            cd.param = jc["param"].GetString();
                            cd.op = ParseOp(jc["op"].GetString());
                            if (jc.HasMember("b") && jc["b"].IsBool())     cd.b = jc["b"].GetBool();
                            if (jc.HasMember("f") && jc["f"].IsNumber())   cd.f = jc["f"].GetFloat();
                            if (jc.HasMember("i") && jc["i"].IsInt())      cd.i = jc["i"].GetInt();
                            tr.conditions.push_back(cd);
                        }
                    }
                    st.transitions.push_back(tr);
                }
            }
            c.states.push_back(st);
        }
        return true;
    }

} // namespace NE::Animation
