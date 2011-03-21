
#include "SqlDataSource.h"
#include "Mapper.h"

using namespace std;

namespace Yb {
namespace ORMapper {

SqlDataSource::SqlDataSource(const TableMetaDataRegistry &reg,
        Yb::SQL::Session &session)
    : reg_(reg)
    , session_(session)
{}

RowDataPtr
SqlDataSource::select_row(const RowData &key)
{
    RowDataVectorPtr vp = select_rows(
            key.get_table().get_name(), FilterByKey(key));
    if (!vp.get() || vp->size() != 1)
        throw ObjectNotFoundByKey("Can't fetch exactly one object.");
    RowDataPtr p(new RowData((*vp)[0]));
    return p;
}

const RowData
SqlDataSource::sql_row2row_data(const string &table_name, const SQL::Row &row)
{
    RowData d(reg_, table_name);
    const TableMetaData &table = reg_.get_table(table_name);
    TableMetaData::Map::const_iterator it = table.begin(), end = table.end();
    for (; it != end; ++it) {
        SQL::Row::const_iterator x = row.find(it->second.get_name());
        if (x == row.end())
            throw FieldNotFoundInFetchedRow(table_name, it->second.get_name());
        Value v(!x->second.is_null() && it->second.get_type() == Value::DateTime ?
                Value(session_.fix_dt_hook(x->second.as_date_time())) :
                x->second);
        d.set(it->second.get_name(), v);
    }
    return d;
}

SQL::RowPtr
SqlDataSource::row_data2sql_row(const RowData &rd)
{
    SQL::RowPtr row(new SQL::Row());
    const TableMetaData &table = rd.get_table();
    TableMetaData::Map::const_iterator it = table.begin(), end = table.end();
    for (; it != end; ++it)
        (*row)[it->second.get_name()] = rd.get(it->second.get_name());
    return row;
}

SQL::RowsPtr
SqlDataSource::row_data_vector2sql_rows(const TableMetaData &table,
        const RowDataVector &rows, int filter)
{
    string seq_name;
    string pk_name = table.find_synth_pk();
    if (!pk_name.empty())
        seq_name = table.get_seq_name();
    SQL::RowsPtr sql_rows(new SQL::Rows());
    RowDataVector::const_iterator it = rows.begin(), end = rows.end();
    for (; it != end; ++it) {
        if (table.get_name() != it->get_table().get_name())
            throw TableDoesNotMatchRow(table.get_name(),
                    it->get_table().get_name());
        if (!seq_name.empty()) {
            PKIDValue pkid = it->get(pk_name).as_pkid();
            if (pkid.is_temp())
                pkid.sync(session_.get_next_value(seq_name));
        }
        if (0 == filter || 1 == filter) {
            bool is_temp = false;
            if (!pk_name.empty()) {
                PKIDValue pkid = it->get(pk_name).as_pkid();
                is_temp = pkid.is_temp();
            }
            if (int(is_temp) != filter)
                continue;
        }
        SQL::RowPtr sql_row = row_data2sql_row(*it);
        sql_rows->push_back(*sql_row);
    }
    return sql_rows;
}

RowDataVectorPtr
SqlDataSource::select_rows(
        const string &table_name, const SQL::Filter &filter, const SQL::StrList &order_by, int max,
        const string &table_alias)
{
    RowDataVectorPtr vp(new RowDataVector);
    SQL::RowsPtr rows = session_.select("*", table_name, filter, SQL::StrList(), SQL::Filter(), order_by, max);
    if (rows.get()) {
        SQL::Rows::const_iterator it = rows->begin(), end = rows->end();
        for (; it != end; ++it)
            vp->push_back(sql_row2row_data(table_alias.empty()? table_name: table_alias, *it));
    }
    return vp;
}

void
SqlDataSource::do_insert_rows(const string &table_name,
        const RowDataVector &rows, bool process_autoinc)
{
    const TableMetaData &table = reg_.get_table(table_name);
    SQL::RowsPtr sql_rows = row_data_vector2sql_rows(table, rows,
            process_autoinc? 1: 0);
    if (!sql_rows->size())
        return;
    string pk_name = table.find_synth_pk();
    SQL::FieldSet excluded;
    TableMetaData::Map::const_iterator it = table.begin(), end = table.end();
    for (; it != end; ++it)
        if ((it->second.is_ro() && !it->second.is_pk()) ||
                (it->first == pk_name && process_autoinc))
            excluded.insert(it->second.get_name());
    vector<long long> new_ids = session_.insert(
            table_name, *sql_rows, excluded, process_autoinc);
    if (process_autoinc) {
        if (new_ids.size() != sql_rows->size())
            throw SQL::NoDataFound("Can't fetch auto-inserted IDs");
        for (int i = 0; i < sql_rows->size(); ++i) {
            PKIDValue pkid = (*sql_rows)[i][pk_name].as_pkid();
            pkid.sync(new_ids[i]);
        }
    }
}

void
SqlDataSource::insert_rows(const string &table_name,
        const RowDataVector &rows)
{
    const TableMetaData &table = reg_.get_table(table_name);
    do_insert_rows(table_name, rows, false);
    if (table.get_autoinc())
        do_insert_rows(table_name, rows, true);
}

void
SqlDataSource::update_rows(const string &table_name,
        const RowDataVector &rows)
{
    const TableMetaData &table = reg_.get_table(table_name);
    SQL::RowsPtr sql_rows = row_data_vector2sql_rows(table, rows);
    SQL::FieldSet excluded, keys;
    TableMetaData::Map::const_iterator it = table.begin(), end = table.end();
    for (; it != end; ++it) {
        if (it->second.is_pk())
            keys.insert(it->second.get_name());
        if (it->second.is_ro() && !it->second.is_pk())
            excluded.insert(it->second.get_name());
    }
    session_.update(table_name, *sql_rows, keys, excluded);
}

void
SqlDataSource::delete_row(const RowData &row)
{
}

long long
SqlDataSource::get_curr_id(const string &seq_name)
{
    return session_.get_curr_value(seq_name);
}

long long
SqlDataSource::get_next_id(const string &seq_name)
{
    return session_.get_next_value(seq_name);
}

} // namespace ORMapper
} // namespace Yb

// vim:ts=4:sts=4:sw=4:et

