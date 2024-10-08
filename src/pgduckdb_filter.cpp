#include "duckdb.hpp"
#include "duckdb/planner/filter/constant_filter.hpp"

extern "C" {
#include "postgres.h"
#include "catalog/pg_type.h"
}

#include "pgduckdb/pgduckdb_filter.hpp"
#include "pgduckdb/pgduckdb_types.hpp"

namespace pgduckdb {

template <class T, class OP>
bool
TemplatedFilterOperation(Datum &value, const duckdb::Value &constant) {
	return OP::Operation((T)value, constant.GetValueUnsafe<T>());
}

template <class OP>
static bool
FilterOperationSwitch(Datum &value, duckdb::Value &constant, Oid type_oid) {
	switch (type_oid) {
	case BOOLOID:
		return TemplatedFilterOperation<bool, OP>(value, constant);
		break;
	case CHAROID:
		return TemplatedFilterOperation<uint8_t, OP>(value, constant);
		break;
	case INT2OID:
		return TemplatedFilterOperation<int16_t, OP>(value, constant);
		break;
	case INT4OID:
		return TemplatedFilterOperation<int32_t, OP>(value, constant);
		break;
	case INT8OID:
		return TemplatedFilterOperation<int64_t, OP>(value, constant);
		break;
	case FLOAT4OID:
		return TemplatedFilterOperation<float, OP>(value, constant);
		break;
	case FLOAT8OID:
		return TemplatedFilterOperation<double, OP>(value, constant);
		break;
	case DATEOID: {
		Datum date_datum = static_cast<int32_t>(value + pgduckdb::PGDUCKDB_DUCK_DATE_OFFSET);
		return TemplatedFilterOperation<int32_t, OP>(date_datum, constant);
		break;
	}
	case TIMESTAMPOID: {
		Datum timestamp_datum = static_cast<int64_t>(value + pgduckdb::PGDUCKDB_DUCK_TIMESTAMP_OFFSET);
		return TemplatedFilterOperation<int64_t, OP>(timestamp_datum, constant);
		break;
	}
	default:
		elog(ERROR, "(DuckDB/FilterOperationSwitch) Unsupported duckdb type: %d", type_oid);
	}
}

bool
ApplyValueFilter(duckdb::TableFilter &filter, Datum &value, bool is_null, Oid type_oid) {
	switch (filter.filter_type) {
	case duckdb::TableFilterType::CONJUNCTION_AND: {
		auto &conjunction = filter.Cast<duckdb::ConjunctionAndFilter>();
		bool value_filter_result = true;
		for (auto &child_filter : conjunction.child_filters) {
			value_filter_result &= ApplyValueFilter(*child_filter, value, is_null, type_oid);
		}
		return value_filter_result;
		break;
	}
	case duckdb::TableFilterType::CONSTANT_COMPARISON: {
		auto &constant_filter = filter.Cast<duckdb::ConstantFilter>();
		switch (constant_filter.comparison_type) {
		case duckdb::ExpressionType::COMPARE_EQUAL:
			return FilterOperationSwitch<duckdb::Equals>(value, constant_filter.constant, type_oid);
			break;
		case duckdb::ExpressionType::COMPARE_LESSTHAN:
			return FilterOperationSwitch<duckdb::LessThan>(value, constant_filter.constant, type_oid);
			break;
		case duckdb::ExpressionType::COMPARE_LESSTHANOREQUALTO:
			return FilterOperationSwitch<duckdb::LessThanEquals>(value, constant_filter.constant, type_oid);
			break;
		case duckdb::ExpressionType::COMPARE_GREATERTHAN:
			return FilterOperationSwitch<duckdb::GreaterThan>(value, constant_filter.constant, type_oid);
			break;
		case duckdb::ExpressionType::COMPARE_GREATERTHANOREQUALTO:
			return FilterOperationSwitch<duckdb::GreaterThanEquals>(value, constant_filter.constant, type_oid);
			break;
		default:
			D_ASSERT(0);
		}
		break;
	}
	case duckdb::TableFilterType::IS_NOT_NULL:
		return is_null == false;
		break;
	case duckdb::TableFilterType::IS_NULL:
		return is_null == true;
		break;
	default:
		D_ASSERT(0);
		break;
	}
}

} // namespace pgduckdb
