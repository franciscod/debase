#pragma once
#include <set>
#include "lib/Toastbox/RuntimeError.h"
#include "lib/nlohmann/json.h"
#include "History.h"
#include "Git.h"

//namespace State {

struct RefState {
    Git::Commit head;
    std::set<Git::Commit> selection;
    std::set<Git::Commit> selectionPrev;
};

using RefHistory = T_History<RefState>;
using CommitHistoryMap = std::map<Git::Commit,RefHistory>;
using RefCommitHistoryMap = std::map<Git::Ref,CommitHistoryMap>;

inline void to_json(nlohmann::json& j, const Git::Commit& x);
inline void from_json(const nlohmann::json& j, Git::Commit& x, Git::Repo repo);

inline void to_json(nlohmann::json& j, const RefState& x);
inline void from_json(const nlohmann::json& j, RefState& x, Git::Repo repo);

inline void to_json(nlohmann::json& j, const RefHistory& x);
inline void from_json(const nlohmann::json& j, RefHistory& x, Git::Repo repo);

#warning TODO: reconsider from_json arg ordering; should the thing being set always be the last argument?

template <typename T>
inline void from_json_vector(const nlohmann::json& j, T& v, Git::Repo repo) {
    using namespace nlohmann;
    using T_Elm = typename T::value_type;
    std::vector<json> elms;
    j.get_to(elms);
    for (const json& j : elms) {
        T_Elm x;
        ::from_json(j, x, repo);
        v.insert(v.end(), x);
    }
}

#include <iostream>

template <typename T>
inline void from_json_map(const nlohmann::json& j, T& m, Git::Repo repo) {
    using namespace nlohmann;
    using T_Key = typename T::key_type;
    using T_Val = typename T::mapped_type;
    std::map<json,json> elms;
    j.get_to(elms);
    for (const auto& i : elms) {
        T_Key key;
        T_Val val;
        ::from_json(i.first, key, repo);
        ::from_json(i.second, val, repo);
        m[key] = val;
    }
}

class RepoState {
public:
    RepoState(Git::Repo repo) : _repo(repo) {}
    
    CommitHistoryMap commitHistoryMap(Git::Ref ref) {
        CommitHistoryMap x;
        auto find = _history.find(ref.fullName());
        if (find != _history.end()) {
            ::from_json_map(find->second, x, _repo);
        }
        return x;
    }
    
    void setCommitHistoryMap(Git::Ref ref, const CommitHistoryMap& x) {
        _Json j = x;
        _history[ref.fullName()] = j;
    }
    
    Git::Repo repo() const {
        return _repo;
    }
    
//private:
    using _Json = nlohmann::json;
    static constexpr uint32_t _Version = 0;
    Git::Repo _repo;
    std::map<_Json,_Json> _history;
};

// MARK: - Commit Serialization
inline void to_json(nlohmann::json& j, const Git::Commit& x) {
    j = Git::StringForId(x.id());
}

inline void from_json(const nlohmann::json& j, Git::Commit& x, Git::Repo repo) {
    std::string id;
    j.get_to(id);
    x = repo.commitLookup(id);
}

//inline void from_json(const nlohmann::json& j, std::deque<Git::Commit>& x, Git::Repo repo) {
//    using namespace nlohmann;
//    std::vector<json> commits;
//    j.get_to(commits);
//    for (const json& j : commits) {
//        ::from_json(j, x.emplace_back(), repo);
//    }
//}

// MARK: - RefState Serialization
inline void to_json(nlohmann::json& j, const RefState& x) {
    j = {
        {"head", x.head},
        {"selection", x.selection},
        {"selectionPrev", x.selectionPrev},
    };
}

inline void from_json(const nlohmann::json& j, RefState& x, Git::Repo repo) {
    ::from_json(j.at("head"), x.head, repo);
    ::from_json_vector(j.at("selection"), x.selection, repo);
    ::from_json_vector(j.at("selectionPrev"), x.selectionPrev, repo);
}

// MARK: - Ref Serialization
inline void from_json(const nlohmann::json& j, Git::Ref& x, Git::Repo repo) {
    std::string name;
    j.get_to(name);
    x = repo.refLookup(name);
}

// MARK: - History Serialization
inline void to_json(nlohmann::json& j, const RefHistory& x) {
    j = {
        {"prev", x._prev},
        {"next", x._next},
        {"current", x._current},
    };
}

inline void from_json(const nlohmann::json& j, RefHistory& x, Git::Repo repo) {
    ::from_json_vector(j.at("prev"), x._prev, repo);
    ::from_json_vector(j.at("next"), x._next, repo);
    ::from_json(j.at("current"), x._current, repo);
}

//// MARK: - Repo Serialization
//inline void to_json(nlohmann::json& j, const Git::Repo& x) {
//    j = std::filesystem::canonical(x.path()).string();
//}
//
//inline void from_json(const nlohmann::json& j, Git::Repo& x) {
//    std::filesystem::path path;
//    j.get_to(path);
//    x = Git::Repo::Open(path);
//}

// MARK: - RepoState Serialization
inline void to_json(nlohmann::json& j, const RepoState& x) {
    j = {
//        {"repo", x.repo},
        {"version", RepoState::_Version},
        {"history", x._history},
    };
    
//    printf("%s\n", j.dump().c_str());
}

inline void from_json(const nlohmann::json& j, RepoState& x, Git::Repo repo) {
//    j.at("repo").get_to(x.repo);
    uint32_t version = 0;
    j.at("version").get_to(version);
    if (version != RepoState::_Version) {
        throw Toastbox::RuntimeError("invalid version (expected %ju, got %ju)",
        (uintmax_t)RepoState::_Version, (uintmax_t)version);
    }
    
    j.at("history").get_to(x._history);
    
//    ::from_json(j.at("history"), x._history);
}



//inline void from_json(const nlohmann::json& j, RefCommitHistory& x, Git::Repo repo) {
//    using namespace nlohmann;
//    std::map<json,json> map;
//    j.get_to(map);
//    for (const auto& i : map) {
//        Git::Ref ref;
//        History s;
//        try {
//            ::from_json(i.first, ref, repo);
//        } catch (...) {
//            // If we fail to deserialize a ref, ignore this History entry
//            continue;
//        }
//        ::from_json(i.second, s, repo);
//        x[ref] = s;
//    }
//}


//// MARK: - State Serialization
//inline void to_json(nlohmann::json& j, const State& x) {
//    j = {
//        {"version", State::Version},
//        {"repoStates", x.repoStates},
//    };
//}
//
//inline void from_json(const nlohmann::json& j, State& x) {
//    uint32_t version = 0;
//    j.at("version").get_to(version);
//    if (version != State::Version) {
//        throw Toastbox::RuntimeError("invalid version (expected %ju, got %ju)",
//        (uintmax_t)State::Version, (uintmax_t)version);
//    }
//    
//    j.at("repoStates").get_to(x.repoStates);
//}


std::filesystem::path ConfigDir();

inline std::filesystem::path RepoStateDir() {
    return ConfigDir() / "RepoState";
}

//} // namespace State
