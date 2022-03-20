#pragma once
#include "RefCounted.h"
#include "lib/Toastbox/RuntimeError.h"
#include "lib/libgit2/include/git2.h"

namespace Git {
using namespace Toastbox;

// Macros to handle checking for nullptr before comparing underlying objects
#define _Equal(a, b, cmp) ({                \
    bool r = ((bool)(a) == (bool)(b));      \
    if ((bool)(a) && (bool)(b)) r = (cmp);  \
    r;                                      \
})

#define _Less(a, b, cmp) ({                 \
    bool r = ((bool)(a) < (bool)(b));       \
    if ((bool)(a) && (bool)(b)) r = (cmp);  \
    r;                                      \
})

inline std::string StringForId(const git_oid& oid) {
    char str[8];
    git_oid_tostr(str, sizeof(str), &oid);
    return str;
}

static const char* _Weekdays[] = { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat", };
static const char* _Months[]   = { "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec", };

struct Time {
    time_t time = 0;
    int offset = 0;
    
    bool operator==(const Time& x) const {
        return time==x.time && offset==x.offset;
    }
};

inline Time TimeForGitTime(const git_time& t) {
    return {
        .time = t.time,
        .offset = t.offset,
    };
}

inline std::string ShortStringForTime(const Time& t) {
    time_t tmp = t.time + t.offset*60;
    
    struct tm tm = {};
    struct tm* tr = gmtime_r(&tmp, &tm);
    if (!tr) throw RuntimeError("gmtime_r failed: %s", strerror(errno));
    
    char buf[64];
    snprintf(buf, sizeof(buf), "%s %s %u %02u:%02u",
        _Weekdays[tm.tm_wday], _Months[tm.tm_mon], tm.tm_mday, tm.tm_hour, tm.tm_min);
    return buf;
}

inline std::string StringFromTime(const Time& t) {
    time_t tmp = t.time + t.offset*60;
    
    struct tm tm = {};
    struct tm* tr = gmtime_r(&tmp, &tm);
    if (!tr) throw RuntimeError("gmtime_r failed: %s", strerror(errno));
    
    char buf[64];
    snprintf(buf, sizeof(buf), "%s %s %d %02d:%02d:%02d %04d %+03d%02d",
        _Weekdays[tm.tm_wday], _Months[tm.tm_mon], tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec, tm.tm_year+1900, t.offset/60, t.offset%60);
    return buf;
}

inline int _WeekdayParse(std::string_view str) {
    for (size_t i=0; i<std::size(_Weekdays); i++) {
        if (str == _Weekdays[i]) {
            return (int)i;
        }
    }
    throw RuntimeError("failed to parse weekday: %s", str);
}

inline int _MonthParse(std::string_view str) {
    for (size_t i=0; i<std::size(_Months); i++) {
        if (str == _Months[i]) {
            return (int)i;
        }
    }
    throw RuntimeError("failed to parse weekday: %s", str);
}

inline Time TimeFromString(std::string_view str) {
    struct tm tm = {};
    
    char weekday[16];
    char month[16];
    int offset = 0;
    int ir = sscanf(str.data(), "%15s %15s %d %d:%d:%d %d %d",
        weekday, month, &tm.tm_mday, &tm.tm_hour, &tm.tm_min, &tm.tm_sec, &tm.tm_year, &offset
    );
    if (ir != 8) throw RuntimeError("sscanf failed");
    
    // Cap string fields to 3 characters
    weekday[3] = 0;
    month[3] = 0;
    
    tm.tm_wday = _WeekdayParse(weekday);
    tm.tm_mon = _MonthParse(month);
    tm.tm_year -= 1900;
    
    int offs = (offset >= 0 ? 1 : -1);
    int offh = std::abs(offset)/100;
    int offm = std::abs(offset)%100;
    offset = offs * offh*60 + offm;
    
    time_t t = timegm(&tm);
    if (t == -1) throw RuntimeError("timegm failed");
    t -= offset*60;
    
    return {
        .time = t,
        .offset = offset,
    };
}

class Error : public std::runtime_error {
public:
    Error(int error, const char* msg) :
    error((git_error_code)error), std::runtime_error(_ErrorMsg(msg)) {}
    
    git_error_code error = GIT_OK;
    
private:
    static std::string _ErrorMsg(const char* msg) {
        char buf[256];
        snprintf(buf, sizeof(buf), "%s: %s", msg, git_error_last()->message);
        return buf;
    }
};

using Id = git_oid;
using Tree = RefCounted<git_tree*, git_tree_free>;
using Index = RefCounted<git_index*, git_index_free>;

struct Signature : RefCounted<git_signature*, git_signature_free> {
    using RefCounted::RefCounted;
    static Signature Create(std::string_view name, std::string_view email, git_time_t time, int offset) {
        git_signature* x = nullptr;
        int ir = git_signature_new(&x, name.data(), email.data(), time, offset);
        if (ir) throw Error(ir, "git_signature_new failed");
        return x;
    }
    
    std::string name() { return (*get())->name; }
    std::string email() { return (*get())->email; }
};

static void _BufDelete(git_buf& buf) { git_buf_dispose(&buf); }
using Buf = RefCounted<git_buf, _BufDelete>;

struct Object : RefCounted<git_object*, git_object_free> {
    using RefCounted::RefCounted;
    const Id& id() const { return *git_object_id(*get()); }
    bool operator==(const Object& x) const { return _Equal(*this, x, git_oid_cmp(&id(), &x.id())==0); }
    bool operator!=(const Object& x) const { return !(*this==x); }
    bool operator<(const Object& x) const { return _Less(*this, x, git_oid_cmp(&id(), &x.id())<0); }
};

struct Config : RefCounted<git_config*, git_config_free> {
    using RefCounted::RefCounted;
    std::optional<std::string> stringGet(std::string_view key) {
        Buf buf;
        {
            git_buf x = GIT_BUF_INIT;
            int ir = git_config_get_string_buf(&x, *get(), key.data());
            if (ir == GIT_ENOTFOUND) return std::nullopt;
            if (ir) throw Error(ir, "git_config_get_string_buf failed");
            buf = x;
        }
        return buf->ptr;
    }
};

struct Commit : Object {
    using Object::Object;
    Commit(const git_commit* x) : Object((git_object*)x) {}
    
    git_commit** get() { return (git_commit**)Object::get(); }
    git_commit*& operator*() { return *get(); }
    
    const git_commit** get() const { return (const git_commit**)Object::get(); }
    const git_commit*& operator*() const { return *get(); }
    
    static Commit FromObject(Object obj) {
        git_commit* x = nullptr;
        int ir = git_object_peel((git_object**)&x, *obj, GIT_OBJECT_COMMIT);
        if (ir) throw Error(ir, "git_object_peel failed");
        return x;
    }
    
    Tree tree() const {
        git_tree* x = nullptr;
        int ir = git_commit_tree(&x, *get());
        if (ir) throw Error(ir, "git_commit_tree failed");
        return x;
    }
    
    Commit parent(size_t n=0) const {
        git_commit* x = nullptr;
        int ir = git_commit_parent(&x, *get(), (unsigned int)n);
        if (ir == GIT_ENOTFOUND) return nullptr; // `this` is the root commit -> no parent exists
        if (ir) throw Error(ir, "git_commit_tree failed");
        return x;
    }
    
    std::vector<Commit> parents() const {
        std::vector<Commit> p;
        size_t n = git_commit_parentcount(*get());
        for (size_t i=0; i<n; i++) p.push_back(parent(i));
        return p;
    }
    
    bool isMerge() const {
        return git_commit_parentcount(*get()) > 1;
    }
    
    const char* idStr() const {
        return git_oid_tostr_s(&id());
    }
};

struct TagAnnotation : Object {
    using Object::Object;
    TagAnnotation(const git_tag* x) : Object((git_object*)x) {}
    const git_tag** get() const { return (const git_tag**)Object::get(); }
    const git_tag*& operator*() const { return *get(); }
    
    Signature author() const {
        const git_signature* author = git_tag_tagger(*get());
        if (!author) return nullptr;
        git_signature* x = nullptr;
        int ir = git_signature_dup(&x, author);
        if (ir) throw Error(ir, "git_signature_dup failed");
        return x;
    }
    
    std::string name() const {
        return git_tag_name(*get());
    }
    
    std::string message() const {
        return git_tag_message(*get());
    }
};

struct Ref : RefCounted<git_reference*, git_reference_free> {
    using RefCounted::RefCounted;
    bool operator==(const Ref& x) const { return _Equal(*this, x, fullName()==x.fullName()); }
    bool operator!=(const Ref& x) const { return !(*this==x); }
    bool operator<(const Ref& x) const { return _Less(*this, x, fullName()<x.fullName()); }
    
    std::string name() const {
        return git_reference_shorthand(*get());
    }
    
    std::string fullName() const {
        return git_reference_name(*get());
    }
    
    bool isBranch() const {
        return git_reference_is_branch(*get());
    }
    
    bool isTag() const {
        return git_reference_is_tag(*get());
    }
    
    Commit commit() const {
        git_commit* x = nullptr;
        int ir = git_reference_peel((git_object**)&x, *get(), GIT_OBJECT_COMMIT);
        if (ir) throw Error(ir, "git_reference_peel failed");
        return x;
    }
    
    Tree tree() const {
        git_tree* x = nullptr;
        int ir = git_reference_peel((git_object**)&x, *get(), GIT_OBJECT_TREE);
        if (ir) throw Error(ir, "git_reference_peel failed");
        return x;
    }
};

struct Branch : Ref {
    using Ref::Ref;
    
    static Branch FromRef(Ref ref) {
        assert(ref.isBranch());
        git_reference* x = nullptr;
        int ir = git_reference_dup(&x, *ref);
        if (ir) throw Error(ir, "git_reference_dup failed");
        return x;
    }
    
    std::string name() const {
        const char* x = nullptr;
        int ir = git_branch_name(&x, *get());
        if (ir) throw Error(ir, "git_branch_name failed");
        return x;
    }
    
    Branch upstream() const {
        git_reference* x = nullptr;
        int ir = git_branch_upstream(&x, *get());
        switch (ir) {
        case 0:             return x;
        case GIT_ENOTFOUND: return nullptr;
        default:            throw Error(ir, "git_branch_upstream failed");
        }
    }
    
    void upstreamSet(Branch upstream) {
        int ir = git_branch_set_upstream(*get(), git_reference_shorthand(*upstream));
        if (ir) throw Error(ir, "git_branch_upstream_name failed");
    }
};

struct Tag : Ref {
    using Ref::Ref;
    
    static Tag FromRef(Ref ref) {
        assert(ref.isTag());
        git_reference* x = nullptr;
        int ir = git_reference_dup(&x, *ref);
        if (ir) throw Error(ir, "git_reference_dup failed");
        return x;
    }
    
    TagAnnotation annotation() const {
        git_tag* x = nullptr;
        int ir = git_reference_peel((git_object**)&x, *get(), GIT_OBJECT_TAG);
        // Allow failures, since we use this method to determine whether the tag is an annotated tag or not
        if (!ir) return x;
        return nullptr;
    }
};

// A Rev holds a mandatory commit, along with an optional reference (ie a branch/tag)
// that targets that commit
class Rev {
public:
    // Invalid Rev
    Rev() {}
    
    Rev(Commit c) : commit(c) {}
    
    Rev(Ref r, size_t skip=0) : ref(r), skip(skip) {
        commit = ref.commit();
        assert(commit); // Verify assumption: if we have a ref, it points to a valid commit
    }
    
    operator bool() const { return (bool)commit; }
    
    bool operator==(const Rev& x) const {
        if (commit != x.commit) return false;
        if (ref != x.ref) return false;
        if (skip != x.skip) return false;
        return true;
    }
    
    bool operator!=(const Rev& x) const { return !(*this==x); }
    
    bool operator<(const Rev& x) const {
        // Stage 1: compare commits
        if (commit != x.commit) return commit<x.commit;
        // Stage 2: compare refs
        if (ref != x.ref) return ref<x.ref;
        // Stage 3: compare skips
        return skip<x.skip;
    }
    
    std::string name() const {
        if (ref) return ref.name() + (skip ? "~" + std::to_string(skip) : "");
        return StringForId(commit.id());
    }
    
    bool isMutable() const {
        assert(*this);
        // Only ref-backed revs are mutable
        if (!ref) return false;
        return ref.isBranch() || ref.isTag();
    }
    
    // head(): returns the head commit considering `skip`
    // skip==0 -> return `commit`
    // skip>0  -> returns the `skip` parent of `commit`
    Commit head() const {
        Commit c = commit;
        size_t s = skip;
        while (c && s) {
            c = c.parent();
            s--;
        }
        return c;
    }
    
    Commit commit;   // Mandatory
    Ref ref;         // Optional
    size_t skip = 0; // Number of commits to skip
};

inline void _RepoFree(git_repository* repo) {
    printf("MyFree()");
    git_repository_free(repo);
}

class Repo : public RefCounted<git_repository*, _RepoFree> {
public:
    using RefCounted::RefCounted;
    
    static Repo Open(std::string_view path) {
        // This git_libgit2_init/git_libgit2_shutdown scheme is a little crazy,
        // but it seems to be the most elegant solution.
        // The problem: we want the lifetime of the `Repo` instance to manage
        // the calls to _init/_shutdown, but _init must be called before
        // git_repository_open. So we call _init ourself to allow us to call
        // git_repository_open, create the Repo instance (which calls _init
        // itself), and finally call _shutdown ourself (via Defer). Then, when
        // ~Repo is called, the final _shutdown will be called.
        git_libgit2_init();
        Defer(git_libgit2_shutdown());
        
        git_repository* x = nullptr;
        int ir = git_repository_open(&x, path.data());
        if (ir) throw Error(ir, "git_repository_open failed");
        return x;
    }
    
//    Repo(git_repository* x) : RefCounted(x) {
//        printf("Repo(%p)\n", *get());
//        git_libgit2_init();
//    }
//    
//    ~Repo() {
//        printf("~Repo(%p)\n", *get());
//        git_libgit2_shutdown();
//    }
    
    Ref head() const {
        git_reference* x = nullptr;
        int ir = git_repository_head(&x, *get());
        if (ir) throw Error(ir, "git_repository_head failed");
        return x;
    }
    
    void headDetach() const {
        int ir = git_repository_detach_head(*get());
        if (ir) throw Error(ir, "git_repository_detach_head failed");
    }
    
    void checkout(std::string_view name) const {
        Ref ref = refFullNameLookup(name);
        const git_checkout_options opts = GIT_CHECKOUT_OPTIONS_INIT;
        int ir = git_checkout_tree(*get(), (git_object*)*ref.tree(), &opts);
        if (ir) throw Error(ir, "git_checkout_tree failed");
        ir = git_repository_set_head(*get(), name.data());
        if (ir) throw Error(ir, "git_repository_set_head failed");
    }
    
    Index treesMerge(Tree ancestorTree, Tree dstTree, Tree srcTree) const {
        git_merge_options mergeOpts = GIT_MERGE_OPTIONS_INIT;
        git_index* x = nullptr;
        int ir = git_merge_trees(
            &x,
            *get(),
            (ancestorTree ? *ancestorTree : nullptr),
            (dstTree ? *dstTree : nullptr),
            (srcTree ? *srcTree : nullptr),
            &mergeOpts
        );
        if (ir) throw Error(ir, "git_merge_trees failed");
        return x;
    }
    
    Tree indexWrite(Index index) const {
        git_oid treeId;
        int ir = git_index_write_tree_to(&treeId, *index, *get());
        if (ir) throw Error(ir, "git_index_write_tree_to failed");
        
        git_tree* x = nullptr;
        ir = git_tree_lookup(&x, *get(), &treeId);
        if (ir) throw Error(ir, "git_tree_lookup failed");
        return x;
    }
    
    // commitParentSet(): commit.parent[0] = parent
    Commit commitParentSet(Commit commit, Commit parent) const {
        assert(commit);
        
        Index index;
        if (parent) {
            index = _cherryPick(parent, commit);
        } else {
            Commit oldParent = commit.parent();
            Tree oldParentTree = (oldParent ? oldParent.tree() : nullptr);
            index = treesMerge(oldParentTree, nullptr, commit.tree());
        }
        
        Tree tree = indexWrite(index);
        
        std::vector<Commit> parents = commit.parents();
        if (!parents.empty()) parents.erase(parents.begin());
        if (parent) parents.insert(parents.begin(), parent);
        
        return commitAmend(commit, parents, tree);
    }
    
    // commitIntegrate: adds the content of `src` into `dst` and returns the result
    Commit commitIntegrate(Commit dst, Commit src) const {
        Tree srcTree = src.tree();
        Tree dstTree = dst.tree();
        Tree ancestorTree = src.parent().tree(); // TODO:MERGE
        Index mergedTreesIndex = treesMerge(ancestorTree, dstTree, srcTree);
        Tree newTree = indexWrite(mergedTreesIndex);
        
        // Combine the commit messages
        std::stringstream msg;
        msg << git_commit_message(*src);
        msg << "\n";
        msg << git_commit_message(*dst);
        
        git_oid id;
        int ir = git_commit_amend(&id, *dst, nullptr, nullptr, nullptr, git_commit_message_encoding(*dst), msg.str().c_str(), *newTree);
        if (ir) throw Error(ir, "git_commit_amend failed");
        return commitLookup(id);
    }
    
    // commitAmend(): change parents/tree of a commit
    Commit commitAmend(Commit commit, std::vector<Commit> parents, Tree tree) const {
        git_oid id;
        
        assert(parents.size() <= 4); // Protect stack-allocated array from being too large
        const git_commit* stackParents[parents.size()];
        for (size_t i=0; i<parents.size(); i++) {
            stackParents[i] = *parents[i];
        }
        
        int ir = git_commit_create(
            &id,
            *get(),
            nullptr,
            git_commit_author(*commit),
            git_commit_committer(*commit),
            git_commit_message_encoding(*commit),
            git_commit_message(*commit),
            *tree,
            parents.size(),
            stackParents
        );
        if (ir) throw Error(ir, "git_commit_create failed");
        return commitLookup(id);
    }
    
    // commitAmend(): change the author/message of a commit
    Commit commitAmend(Commit commit, Signature author, std::string_view msg) const {
        git_oid id;
        int ir = git_commit_amend(&id, *commit, nullptr, *author, nullptr,
            nullptr, (!msg.empty() ? msg.data() : nullptr), nullptr);
        if (ir) throw Error(ir, "git_commit_amend failed");
        return commitLookup(id);
    }
    
    Commit commitLookup(const Id& id) const {
        git_commit* x = nullptr;
        int ir = git_commit_lookup(&x, *get(), &id);
        if (ir) throw Error(ir, "git_commit_lookup failed");
        return x;
    }
    
    Commit commitLookup(std::string_view idStr) const {
        Id id;
        int ir = git_oid_fromstr(&id, idStr.data());
        if (ir) throw Error(ir, "git_oid_fromstr failed");
        return commitLookup(id);
    }
    
    Ref refReplace(Ref ref, Commit commit) const {
        if (ref.isBranch()) {
            return branchReplace(Branch::FromRef(ref), commit);
        
        } else if (ref.isTag()) {
            return tagReplace(Tag::FromRef(ref), commit);
//            return refReload(ref);
//            return tagReplace(ref.tag(), commit);
        
        } else {
            // Unknown ref type
            abort();
        }
    }
    
    Branch branchReplace(Branch branch, Commit commit) const {
        Branch upstream = branch.upstream();
        Branch newBranch = branchCreate(branch.name(), commit, true);
        Branch newBranchUpstream = newBranch.upstream();
        
        if (upstream && !newBranchUpstream) {
            newBranch.upstreamSet(upstream);
        }
        
        return newBranch;
    }
    
    Tag tagReplace(Tag tag, Commit commit) const {
        if (TagAnnotation ann = tag.annotation()) {
            return tagCreateAnnotated(tag.name(), commit, ann.author(), ann.message(), true);
        }
        return tagCreate(tag.name(), commit, true);
    }
    
    Ref refLookup(std::string_view name) const {
        git_reference* x = nullptr;
        int ir = git_reference_dwim(&x, *get(), name.data());
        if (ir) throw Error(ir, "git_reference_dwim failed");
        return x;
    }
    
    Ref refFullNameLookup(std::string_view name) const {
        git_reference* x = nullptr;
        int ir = git_reference_lookup(&x, *get(), name.data());
        if (ir) throw Error(ir, "git_reference_dwim failed");
        return x;
    }
    
    Ref refReload(Ref ref) const {
        return refFullNameLookup(ref.fullName());
    }
    
    Rev revLookup(std::string_view str) {
        Rev origRev = _revLookup(str);
        if (origRev.ref) return origRev;
        
        // Try to get a ref-backed rev (with a skip parameter) by removing the ^ or ~ suffix from `str`
        Rev skipRev;
        if (!skipRev.ref) {
            std::string name(str);
            size_t pos = name.find("^");
            if (pos != std::string::npos) {
                name.erase(pos);
                skipRev = _revLookup(name);
            }
        }
        
        if (!skipRev.ref) {
            std::string name(str);
            size_t pos = name.find("~");
            if (pos != std::string::npos) {
                name.erase(pos);
                skipRev = _revLookup(name);
            }
        }
        
        // If we found a `skipRev` by removing the ^~ suffix, calculate the `skip` value
        if (skipRev.ref) {
            size_t skip = 0;
            Commit head = skipRev.commit;
            while (head && head!=origRev.commit) {
                head = head.parent();
                skip++;
            }
            
            if (head) return Rev(skipRev.ref, skip);
        }
        
        return origRev;
    }
    
    Branch branchLookup(std::string_view name) const {
        git_reference* x = nullptr;
        int ir = git_branch_lookup(&x, *get(), name.data(), GIT_BRANCH_LOCAL);
        if (ir) throw Error(ir, "git_branch_lookup failed");
        return x;
    }
    
    Branch branchCreate(std::string_view name, Commit commit, bool force=false) const {
        git_reference* x = nullptr;
        int ir = git_branch_create(&x, *get(), name.data(), (commit ? *commit : nullptr), force);
        if (ir) throw Error(ir, "git_branch_create failed");
        return x;
    }
    
    Tag tagLookup(std::string_view name) const {
        return Tag::FromRef(refLookup(name));
    }
    
    Tag tagCreate(std::string_view name, Commit commit, bool force=false) const {
        git_oid id;
        int ir = git_tag_create_lightweight(&id, *get(), name.data(), *(Object)commit, force);
        if (ir) throw Error(ir, "git_tag_create failed");
        return tagLookup(name);
    }
    
    Tag tagCreateAnnotated(std::string_view name, Commit commit, Signature author, std::string_view message, bool force=false) const {
        git_oid id;
        int ir = git_tag_create(&id, *get(), name.data(), *((Object)commit), *author, message.data(), force);
        if (ir) throw Error(ir, "git_tag_create failed");
        return tagLookup(name);
    }
    
    Config config() const {
        git_config* x = nullptr;
        int ir = git_repository_config(&x, *get());
        if (ir) throw Error(ir, "git_repository_config failed");
        return x;
    }

private:
    Rev _revLookup(std::string_view str) const {
        Object obj;
        Ref ref;
        
        // Determine whether `str` is a ref or commit
        {
            git_object* po = nullptr;
            git_reference* pr = nullptr;
            int ir = git_revparse_ext(&po, &pr, *get(), str.data());
            if (ir) throw Error(ir, "git_revparse_ext failed");
            obj = po;
            ref = pr;
        }
        
        if (ref) return Rev(ref);
        return Rev(Commit::FromObject(obj));
    }
    
    Index _cherryPick(Commit dst, Commit commit) const {
        unsigned int mainline = (commit.isMerge() ? 1 : 0);
        git_merge_options opts = GIT_MERGE_OPTIONS_INIT;
        opts.file_favor = GIT_MERGE_FILE_FAVOR_THEIRS;
        
        git_index* x = nullptr;
        int ir = git_cherrypick_commit(&x, *get(), *commit, *dst, mainline, &opts);
        if (ir) throw Error(ir, "git_cherrypick_commit failed");
        return x;
    }
};

#undef _Equal
#undef _Less

} // namespace Git
