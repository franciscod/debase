#pragma once
#include <deque>
#include "lib/Toastbox/RuntimeError.h"
#include "lib/nlohmann/json.h"
#include "UndoState.h"
#include "Git.h"

using UndoState = T_UndoState<Git::Commit>;

struct RepoState {
    static constexpr uint32_t Version = 0;
//    std::filesystem::path repo;
    std::map<Git::Ref,UndoState> undoStates;
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

inline void from_json(const nlohmann::json& j, std::deque<Git::Commit>& x, Git::Repo repo) {
    using namespace nlohmann;
    std::vector<json> commits;
    j.get_to(commits);
    for (const json& j : commits) {
        ::from_json(j, x.emplace_back(), repo);
    }
}

// MARK: - Ref Serialization
inline void from_json(const nlohmann::json& j, Git::Ref& x, Git::Repo repo) {
    std::string name;
    j.get_to(name);
    x = repo.refLookup(name);
}

// MARK: - UndoState Serialization
inline void to_json(nlohmann::json& j, const UndoState& x) {
    j = {
        {"undo", x._undo},
        {"redo", x._redo},
        {"current", x._current},
    };
}

inline void from_json(const nlohmann::json& j, UndoState& x, Git::Repo repo) {
    ::from_json(j.at("undo"), x._undo, repo);
    ::from_json(j.at("redo"), x._redo, repo);
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
        {"version", RepoState::Version},
        {"undoStates", x.undoStates},
    };
    
    printf("%s\n", j.dump().c_str());
}

inline void from_json(const nlohmann::json& j, std::map<Git::Ref,UndoState>& x, Git::Repo repo) {
    using namespace nlohmann;
    std::map<json,json> map;
    j.get_to(map);
    for (const auto& i : map) {
        Git::Ref ref;
        UndoState s;
        ::from_json(i.first, ref, repo);
        ::from_json(i.second, s, repo);
        x[ref] = s;
    }
}

inline void from_json(const nlohmann::json& j, RepoState& x, Git::Repo repo) {
//    j.at("repo").get_to(x.repo);
    uint32_t version = 0;
    j.at("version").get_to(version);
    if (version != RepoState::Version) {
        throw Toastbox::RuntimeError("invalid version (expected %ju, got %ju)",
        (uintmax_t)RepoState::Version, (uintmax_t)version);
    }
    
    ::from_json(j.at("undoStates"), x.undoStates, repo);
}

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
