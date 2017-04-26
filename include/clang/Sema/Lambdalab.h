#ifndef Lambdalab_h
#define Lambdalab_h

#include <unordered_map>

namespace lambdalab {
    class UnresolvedTypeInfo {
    public:
        UnresolvedTypeInfo() = default;
        UnresolvedTypeInfo(const std::string& Name, const clang::SourceLocation& Location)
        :Loc(Location),Name(Name){}
        clang::SourceLocation Loc;
        std::string Name;
    };
    struct UnresolvedVarTable {
        std::unordered_map<const clang::NamedDecl*, UnresolvedTypeInfo> Map;
        bool HasUnresolvedName;
        UnresolvedTypeInfo Cur;
        UnresolvedVarTable() = default;
    };
    class UnresolvedVarRecorder {
    private:
        static UnresolvedVarTable& getUnresolvedVarTable() {
            static UnresolvedVarTable Table;
            return Table;
        }
    public:
        static void recordType(const UnresolvedTypeInfo& CurrentType){
            auto &Table = getUnresolvedVarTable();
            Table.Cur = CurrentType;
            Table.HasUnresolvedName = true;
        }
        static void addFeildDecl(const clang::NamedDecl* D) {
            auto &Table = getUnresolvedVarTable();
            assert(Table.HasUnresolvedName);
            Table.Map.insert({D, Table.Cur});
        }
        static UnresolvedTypeInfo getInfo(const clang::NamedDecl* D) {
            auto &Table = getUnresolvedVarTable();
            auto &Map = Table.Map;
            auto Slot = Map.find(D);
            assert(Slot != Map.end());
            return Slot->second;
        }
        static void clear(){
            auto &Table = getUnresolvedVarTable();
            Table.Map.clear();
            Table.HasUnresolvedName = false;
        }
        static bool contains(const clang::NamedDecl* D){
            auto &Map = getUnresolvedVarTable().Map;
            return Map.find(D) != Map.end();
        }
    };
}

#endif /* Lambdalab_h */
