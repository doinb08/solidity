/*
	This file is part of solidity.

	solidity is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	solidity is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with solidity.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <libsolidity/formal/Z3CHCInterface.h>

#include <liblangutil/Exceptions.h>
#include <libdevcore/CommonIO.h>

using namespace std;
using namespace dev;
using namespace dev::solidity::smt;

Z3CHCInterface::Z3CHCInterface():
	m_z3Interface(make_unique<Z3Interface>()),
	m_context(m_z3Interface->context()),
	m_solver(*m_context)
{
	// These need to be set globally.
	z3::set_param("rewriter.pull_cheap_ite", true);
	z3::set_param("rlimit", Z3Interface::resourceLimit);

	// Spacer options.
	// These needs to be set in the solver.
	// https://github.com/Z3Prover/z3/blob/master/src/muz/base/fp_params.pyg
	z3::params p(*m_context);
	// These are useful for solving problems with arrays and loops.
	// Use quantified lemma generalizer.
	p.set("fp.spacer.q3.use_qgen", true);
	p.set("fp.spacer.mbqi", false);
	// Ground pobs by using values from a model.
	p.set("fp.spacer.ground_pobs", false);
	// These reduce performance but are needed for counterexamples.
	// They might be removed when we use get_proof instead of get_answer.
	p.set("fp.xform.slice", false);
	p.set("fp.xform.inline_linear", false);
	p.set("fp.xform.inline_eager", false);
	m_solver.set(p);
}

void Z3CHCInterface::declareVariable(string const& _name, SortPointer const& _sort)
{
	solAssert(_sort, "");
	m_z3Interface->declareVariable(_name, _sort);
}

void Z3CHCInterface::registerRelation(Expression const& _expr)
{
	m_solver.register_relation(m_z3Interface->functions().at(_expr.name));
}

void Z3CHCInterface::addRule(Expression const& _expr, string const& _name)
{
	z3::expr rule = m_z3Interface->toZ3Expr(_expr);
	cout << rule << "\n\n";
	if (m_z3Interface->constants().empty())
		m_solver.add_rule(rule, m_context->str_symbol(_name.c_str()));
	else
	{
		z3::expr_vector variables(*m_context);
		for (auto const& var: m_z3Interface->constants())
			variables.push_back(var.second);
		z3::expr boundRule = z3::forall(variables, rule);
		m_solver.add_rule(boundRule, m_context->str_symbol(_name.c_str()));
	}
}

pair<CheckResult, vector<string>> Z3CHCInterface::query(Expression const& _expr)
{
	//cout << m_solver << endl;
	CheckResult result;
	vector<string> values;
	try
	{
		z3::expr z3Expr = m_z3Interface->toZ3Expr(_expr);
		switch (m_solver.query(z3Expr))
		{
		case z3::check_result::sat:
		{
			cout << "SAT\n" << m_solver.get_answer() << endl;
			result = CheckResult::SATISFIABLE;
			values = parseCounterexample(m_solver.get_answer());
			// TODO retrieve model.
			break;
		}
		case z3::check_result::unsat:
		{
			cout << "UNSAT\n" << m_solver.get_answer() << endl;
			result = CheckResult::UNSATISFIABLE;
			// TODO retrieve invariants.
			break;
		}
		case z3::check_result::unknown:
		{
			cout << "UNKNOWN\n" << m_solver.reason_unknown() << endl;
			result = CheckResult::UNKNOWN;
			break;
		}
		}
		// TODO retrieve model / invariants
	}
	catch (z3::exception const& _e)
	{
		cout << _e.msg() << endl;
		result = CheckResult::ERROR;
		values.clear();
	}

	return make_pair(result, values);
}

vector<string> Z3CHCInterface::parseCounterexample(z3::expr const&/* _cex*/) const
{
	auto trace = Z3_fixedpoint_get_rules_along_trace(*m_context, m_solver);
	cout << Z3_ast_vector_to_string(*m_context, trace) << endl;
	vector<string> model;
	/*
	solAssert(_cex.is_and(), "");
	for (unsigned i = 0; i < _cex.num_args(); ++i)
	{
		auto const& predicate = _cex.arg(i);
		solAssert(predicate.is_app(), "");
		auto const& name = predicate.decl().name().str();
		if (name.find("summary", 0) != 0)
			continue;
		for (unsigned j = 0; j < predicate.num_args(); ++j)
			model.push_back(predicate.arg(j).to_string());
	}
	solAssert(!model.empty(), "");
	*/
	return model;
}
