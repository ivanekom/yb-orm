#include <iostream>
#include <map>
#include <fstream>
#include <util/str_utils.hpp>
//#include <yb/logger/logger.hpp>
#include "orm/MetaData.h"
#include "orm/Value.h"
#include "orm/XMLMetaDataConfig.h"

//#define ORM_LOG(x) LOG(LogLevel::INFO, "ORM Domain generator: " << x)
#define ORM_LOG(x) std::cout << "ORM Domain generator: " << x << "\n";

using namespace Yb::StrUtils;
using namespace Yb::ORMapper;
using namespace std;
namespace Yb {

class OrmDomainGenerator
{
public:
    OrmDomainGenerator(const std::string &path, const TableMetaDataRegistry &reg)
        : path_(path)
        , reg_(reg)
        , mem_weak(false)
    {}
    void generate(const XMLMetaDataConfig &cfg)
    {
        ORM_LOG("generation started...");
        TableMetaDataRegistry::Map::const_iterator it = reg_.begin(), end = reg_.end();
        for (; it != end; ++it)
            if(cfg.need_generation(it->first))
                generate_table(it->second);
    }
private:
    void init_weak(const TableMetaData &t)
    {
        mem_weak = false;
        try {
            reg_.get_table(t.get_name()).get_synth_pk();
        } catch (const NotSuitableForAutoCreating &) {
            mem_weak = true;
        }
    }
    void write_header(const TableMetaData &t, std::ostream &str)
    {
        init_weak(t);
        string name = "__ORM_DOMAIN__" + str_to_upper(t.get_name().substr(2)) + "_H_";
        str << "#ifndef " << name << "\n"
            << "#define " << name << "\n\n";
        if(!mem_weak)
            str << "#include \"utils/Exceptions.h\"\n";
        str << "#include \"orm/DataObj.h\"\n"
            << "#include \"domain2/AutoXMLizable.h\"\n";
        write_include_dependencies(t, str);
        str << "\nnamespace Yb {\n"
            << "namespace Domain {\n\n";
        if (!mem_weak) {
            str << "class " << get_file_class_name(t.get_name()) << "NotFoundByID: public NotFound\n"
                << "{\n"
                << "public:\n"
                << "\t" << get_file_class_name(t.get_name()) << "NotFoundByID(long long id)\n"
                << "\t\t:NotFound(" << t.get_name().substr(2, t.get_name().size()-2) << "_NOT_FOUND" ", \""
                << get_file_class_name(t.get_name()) << " with ID = $OBJECT_ID$ not found\", id)\n"
                << "\t{}\n"
                << "};\n\n";
        }
        str << "class " << get_file_class_name(t.get_name()) << ": public AutoXMLizable\n"
            << "{\n"
            << "\tORMapper::Mapper *mapper_;\n";
        write_members(t, str);

        str << "public:\n"
            << "\t// static method 'find'\n"
            << "\ttypedef std::vector<" << get_file_class_name(t.get_name()) << "> List;\n"
            << "\ttypedef std::auto_ptr<List> ListPtr;\n"
            << "\tstatic ListPtr find(ORMapper::Mapper &mapper,\n"
            << "\t\t\tconst SQL::Filter &filter, const SQL::StrList order_by = \"\", int max_n = -1);\n";

        str << "\t// constructors\n"
            << "\t" << get_file_class_name(t.get_name()) << "()\n"
            << "\t\t:mapper_(NULL)\n"
            << "\t{}\n";
        str << "\t" << get_file_class_name(t.get_name()) << "(ORMapper::Mapper &mapper, const ORMapper::RowData &key)\n"
            << "\t\t:mapper_(&mapper)\n"
            << "\t\t," << get_member_name(t.get_name()) << "(mapper, key)\n"
            << "\t" << "{}\n";
        try {
            std::string mega_key = reg_.get_table(t.get_name()).get_unique_pk();
            str << "\t" << get_file_class_name(t.get_name()) << "(ORMapper::Mapper &mapper, long long id)\n"
                << "\t\t:mapper_(&mapper)\n"
                << "\t\t," << get_member_name(t.get_name()) << "(mapper, \"" << t.get_name() << "\", id)\n"
                << "\t{}\n";
        }
        catch (const AmbiguousPK &) {}
        str << "\t" << get_file_class_name(t.get_name()) << "(ORMapper::Mapper &mapper);\n\n";
        
        str << "\tvoid set(const std::string &field, const Value &val) {\n"
            << "\t\t" << get_member_name(t.get_name()) << ".set(field, val);\n"
            << "\t}\n";

        if (mem_weak) {
            str << "\tconst Value get(const std::string &field) const {\n"
                << "\t\treturn " << get_member_name(t.get_name()) << ".get(field);\n"
                << "\t}\n";
        }
        else {
            str << "\tconst Value get(const std::string &field) const {\n"
                << "\t\ttry {\n"
                << "\t\t\treturn " << get_member_name(t.get_name()) << ".get(field);\n"
                << "\t\t}\n"
                << "\t\tcatch (const ORMapper::ObjectNotFoundByKey &) {\n"
                << "\t\t\tthrow " << get_file_class_name(t.get_name()) << "NotFoundByID("
                << get_member_name(t.get_name()) << ".get(\"ID\").as_long_long());\n"
                << "\t\t}\n"
                << "\t}\n";
        }
        str << "\tconst ORMapper::XMLNode auto_xmlize(int deep = 0) const\n"
            << "\t{\n"
            << "\t\treturn " << get_member_name(t.get_name()) << ".auto_xmlize(*mapper_, deep);\n"
            << "\t}\n";
    }

    void write_include_dependencies(const TableMetaData &t, std::ostream &str)
    {
        TableMetaData::Map::const_iterator it = t.begin(), end = t.end();
        for (; it != end; ++it)
            if (it->second.has_fk())
                str << "#include \"domain2/" << get_file_class_name(it->second.get_fk_table_name()) << ".h\"\n";
    }

    void write_members(const TableMetaData &t, std::ostream &str)
    {
        str << (mem_weak ? "\tWeakObject " : "\tStrongObject ") << get_member_name(t.get_name()) << ";\n";
    }

    void write_footer(std::ostream &str)
    {
        str << "};\n\n"
            << "} // namespace Domain\n"
            << "} // namespace Yb\n\n"
            << "// vim:ts=4:sts=4:sw=4:et\n"
            << "#endif\n";
    }

    void write_cpp_data(const TableMetaData &t, std::ostream &str)
    {
        str << "#include \"domain2/" << get_file_class_name(t.get_name()) << ".h\"\n"
            << "#include \"orm/DomainFactorySingleton.h\"\n\n"
            << "namespace Yb {\n"
            << "namespace Domain {\n\n";

// Constructor for creating new objects

        str << get_file_class_name(t.get_name()) << "::" 
            << get_file_class_name(t.get_name()) << "(ORMapper::Mapper &mapper)\n"
            << "\t:mapper_(&mapper)\n"
            << "\t," << get_member_name(t.get_name()) << "(mapper, \"" << t.get_name() << "\")\n";
        
        if(mem_weak) {
            str << "{}\n";
        } 
        else {
            str << "{\n";
            TableMetaData::Map::const_iterator it = t.begin(), end = t.end();
            for (; it != end; ++it)
            {
                Yb::Value def_val = it->second.get_default_value();
                if (!def_val.is_null()) {
                    switch (it->second.get_type()) {
                        case Value::LongLong:
                            str << "\t" << get_member_name(t.get_name()) 
                                << ".set(\"" << it->first << "\", Value(" << def_val.as_string() << "LL));\n"; 
                            break;
                        case Value::Decimal:
                            str << "\t" << get_member_name(t.get_name()) 
                                << ".set(\"" << it->first << "\", Value(decimal(" << def_val.as_string() << ")));\n"; 
                            break;
                        case Value::DateTime:
                            str << "\t" << get_member_name(t.get_name()) 
                                << ".set(\"" << it->first << "\", Value(boost::posix_time::second_clock::local_time()));\n"; 
                            break;
                    }
                }
            }
            str << "}\n";
        }

        str << get_file_class_name(t.get_name()) << "::ListPtr\n"
            << get_file_class_name(t.get_name()) << "::find(ORMapper::Mapper &mapper,\n"
            << "\t\tconst SQL::Filter &filter, const SQL::StrList order_by, int max_n)\n"
            << "{\n"
            << "\t" << get_file_class_name(t.get_name()) << "::ListPtr lst(new "
            << get_file_class_name(t.get_name()) << "::List());\n"
            << "\tORMapper::LoadedRows rows = mapper.load_collection(\""
            << t.get_name() << "\", filter, order_by, max_n);\n"
            << "\tif (rows.get()) {\n"
            << "\t\tstd::vector<ORMapper::RowData * > ::const_iterator it = rows->begin(), end = rows->end();\n"
            << "\t\tfor (; it != end; ++it)\n"
            << "\t\t\tlst->push_back(" << get_file_class_name(t.get_name()) << "(mapper, **it));\n"
            << "\t}\n"
            << "\treturn lst;\n"
            << "}\n\n"
            << "struct "<< get_file_class_name(t.get_name()) << "Registrator\n{\n"
            << "\t" << get_file_class_name(t.get_name()) << "Registrator()\n\t{\n"
            << "\t\ttheDomainFactory::Instance().register_creator(\"" << t.get_name() << "\",\n"
            << "\t\t\tORMapper::CreatorPtr(new ORMapper::DomainCreator<" << get_file_class_name(t.get_name()) << ">()));\n"
            << "\t}\n"
            << "};\n\n"
            << "static " << get_file_class_name(t.get_name()) << "Registrator " << get_member_name(t.get_name()) << "registrator;\n\n"
            << "} // end namespace Domain\n"
            << "} // end namespace Yb\n\n"
            << "// vim:ts=4:sts=4:sw=4:et\n";
    }

    void expand_tabs_to_stream(const std::string &in, std::ostream &out)
    {
        string::const_iterator it = in.begin(), end = in.end();
        for (; it != end; ++it) {
            if (*it == '\t')
                out << "    ";
            else
                out << *it;
        }
    }

    void generate_table(const TableMetaData &table)
    {
        string file_path = path_ + "/" + get_file_class_name(table.get_name()) + ".h";
        ORM_LOG("Generating file: " << file_path << " for table '" << table.get_name() << "'");
        std::ostringstream header;
        write_header(table, header);
        write_getters(table, header);
        write_setters(table, header);
        write_is_nulls(table, header);
        write_foreign_keys_link(table, header);
        write_footer(header);
        ofstream file(file_path.c_str());
        expand_tabs_to_stream(header.str(), file);

        string cpp_path = path_ + "/" + get_file_class_name(table.get_name()) + ".cpp";
        ORM_LOG("Generating cpp file: " << cpp_path << " for table '" << table.get_name() << "'");
        std::ostringstream cpp;
        write_cpp_data(table, cpp);
        ofstream cpp_file(cpp_path.c_str());
        expand_tabs_to_stream(cpp.str(), cpp_file);
    }

    void write_is_nulls(const TableMetaData &t, std::ostream &str) {
        str << "\t// on null checkers\n";
        TableMetaData::Map::const_iterator it = t.begin(), end = t.end();
        for (; it != end; ++it)
            if (!it->second.has_fk()) {
                str << "\tbool is_" << str_to_lower(it->second.get_name()) << "_null() const {\n"
                    << "\t\treturn " << "get(\"" << it->second.get_name() << "\")" << ".is_null();\n"
                    << "\t}\n";
            }
    }
    void write_getters(const TableMetaData &t, std::ostream &str)
    {
        str << "\t// getters\n";
        TableMetaData::Map::const_iterator it = t.begin(), end = t.end();
        for (; it != end; ++it)
            if (!it->second.has_fk()) {
                if (it->second.get_type() == Value::String) {
                    str << "\t" << type_by_handle(it->second.get_type())
                        << " get_" << str_to_lower(it->second.get_name()) << "() const {\n"
                        << "\t\tValue v(" << "get(\"" << it->second.get_name() << "\"));\n"
                        << "\t\treturn v.is_null()? std::string(): v.as_string();\n"
                        << "\t}\n";
                } 
                else {
                    str << "\t" << type_by_handle(it->second.get_type())
                        << " get_" << str_to_lower(it->second.get_name()) << "() const {\n"
                        << "\t\treturn " << "get(\"" << it->second.get_name() << "\")"
                        << "." << value_type_by_handle(it->second.get_type()) << ";\n"
                        << "\t}\n";
                }
            }
    }

    void write_setters(const TableMetaData &t, std::ostream &str)
    {
        str << "\t// setters\n";
        TableMetaData::Map::const_iterator it = t.begin(), end = t.end();
        for (; it != end; ++it)
            if (!it->second.has_fk() && !it->second.is_ro()) {
                str << "\tvoid set_" << str_to_lower(it->second.get_name())
                    << "(" << type_by_handle(it->second.get_type())
                    << (it->second.get_type() == Value::String ? " &" : " ")
                    << str_to_lower(it->second.get_name()) << "__) {\n"
                    << "\t\tset(\"" << it->second.get_name() << "\", Value("
                    << str_to_lower(it->second.get_name()) << "__));\n"
                    << "\t}\n";
            }
    }

    string type_by_handle(int type)
    {
        switch (type) {
            case Value::LongLong:
                return "long long";
            case Value::DateTime:
                return "boost::posix_time::ptime";
            case Value::String:
                return "const std::string";
            case Value::Decimal:
                return "decimal";
            default:
                throw std::runtime_error("Unknown type while parsing metadata");
        }
    }

    string value_type_by_handle(int type)
    {
        switch (type) {
            case Value::LongLong:
                return "as_long_long()";
            case Value::DateTime:
                return "as_date_time()";
            case Value::String:
                return "as_string()";
            case Value::Decimal:
                return "as_decimal()";
            default:
                throw std::runtime_error("Unknown type while parsing metadata");
        }
    }

    void write_foreign_keys_link(const TableMetaData &t, std::ostream &str)
    {
        TableMetaData::Map::const_iterator it = t.begin(), end = t.end();
        typedef std::map<std::string, std::string> MapString;
        MapString map_fk;
        for (; it != end; ++it) {
            if(it->second.has_fk()) {
                string name = get_member_name(it->second.get_fk_table_name()).substr(0, it->second.get_fk_table_name().size()-2);
                MapString::iterator found = map_fk.find(name);
                if (found != map_fk.end()) {
                    std::string new_entity_name = get_entity_name_by_field(it->second.get_name());
                    if(new_entity_name == name) {
                        std::string field = found->second;
                        map_fk.erase(found);
                        map_fk.insert(MapString::value_type(get_entity_name_by_field(field), field));
                        map_fk.insert(MapString::value_type(name, it->second.get_name()));
                    }
                    else
                        map_fk.insert(MapString::value_type(new_entity_name, it->second.get_name()));
                }
                else
                    map_fk.insert(MapString::value_type(name, it->second.get_name()));
            }
        }

        MapString reverse_field_fk;
        MapString::const_iterator m_it = map_fk.begin(), m_end = map_fk.end();
        for (; m_it != m_end; ++m_it) {
            reverse_field_fk.insert(MapString::value_type(m_it->second, m_it->first));
        }
        if(reverse_field_fk.size()>0)
            str << "\t// foreign key operations\n";

        it = t.begin(), end = t.end();
        for (; it != end; ++it)
            if (it->second.has_fk())
            {
                string name = reverse_field_fk.find(it->second.get_name())->second;
                string fk_name = str_to_lower(it->second.get_fk_name());
                if(it->second.is_nullable()) {
                    str << "\tbool has_" << name << "() const {\n"
                        << "\t\treturn !get(\"" << it->second.get_name() << "\").is_null();\n"
                        << "\t}\n";

                    str << "\tvoid reset_" << name << "() {\n"
                        << "\t\tset(\""
                        << it->second.get_name() << "\", Value());\n"
                        << "\t}\n";
                }
                str << "\tvoid set_" << name << "(const "
                    << get_file_class_name(it->second.get_fk_table_name())
                    << " &" << name << ") {\n"
                    << "\t\tset(\"" << it->second.get_name() << "\", Value("
                    << name << ".get_" << fk_name << "()));\n"
                    << "\t}\n";

                str << "\tconst " << get_file_class_name(it->second.get_fk_table_name())
                    <<  " get_" << name << "() const {\n"
                    << "\t\treturn " << get_file_class_name(it->second.get_fk_table_name())
                    << "(*mapper_, get(\"" << it->second.get_name() << "\").as_long_long());\n"
                    << "\t}\n";
            }
    }

    std::string get_entity_name_by_field(const std::string &field)
    {
        return str_to_lower(field.substr(0, field.size()-3));
    }

    std::string get_file_class_name(const std::string &table_name)
    {
        string result;
        result.push_back(to_upper(table_name[2]));
        for (size_t i = 3; i< table_name.size(); ++i) {
            if (table_name[i] == '_' && ((i+1) < table_name.size())) {
                result.push_back(to_upper(table_name[i+1]));
                ++i;
            } else {
                result.push_back(to_lower(table_name[i]));
            }
        }
        return result;
    }

    std::string get_member_name(const std::string &table_name)
    {
        return str_to_lower(table_name.substr(2, table_name.size()-2)) + "_";
    }

    string path_;
    const TableMetaDataRegistry &reg_;
    bool mem_weak;
};

} // end of namespace Yb

using namespace Yb;

int main(int argc, char *argv[])
{
    if(argc < 3)
    {
        std::cerr << "orm_domain_generator config.xml output_path " << std::endl;
        return 0;
    }
    try {
        string config = argv[1];
        string output_path = argv[2];
        string config_contents;
        load_xml_file(config, config_contents);
        XMLMetaDataConfig cfg(config_contents);
        TableMetaDataRegistry r;
        cfg.parse(r);
        r.check();
        OrmDomainGenerator generator(output_path, r);
        generator.generate(cfg);
        ORM_LOG("generation successfully finished");
    }
    catch (std::exception &e) {
        string error = string("Exception: ") + e.what();
        std::cerr << error << endl;
        ORM_LOG(error);
    }
    catch (...) {
        std::cerr << "Unknown exception" << endl;
        ORM_LOG("Unknown exception");
    }
    return 0;
}

