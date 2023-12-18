#include "check.hpp"

#include <algorithm>

#include <regex>

#include <json/json.hpp>
#include <logicexpr/logicexpr.hpp>

#include "builders/baseHelper.hpp"
#include "builders/helperParser.hpp"
#include "syntax.hpp"

namespace builder::builders
{
namespace
{
using namespace builder::builders::detail;

base::Expression checkListBuilder(const std::vector<json::Json>& list, const std::shared_ptr<const IBuildCtx>& buildCtx)
{
    std::vector<base::Expression> conditionExpressions;
    std::transform(list.begin(),
                   list.end(),
                   std::back_inserter(conditionExpressions),
                   [buildCtx](const auto& condition)
                   {
                       auto opExpr = baseHelperBuiler(condition, buildCtx, builders::HelperType::FILTER);
                       return opExpr;
                   });

    auto expression = base::And::create("stage.check", conditionExpressions); // TODO name?

    return expression;
}

std::function<std::function<bool(base::Event)>(const BuildToken&)>
getTermBuilder(const std::shared_ptr<const IBuildCtx>& buildCtx)
{
    return [buildCtx](const BuildToken& token)
    {
        std::function<bool(base::Event)> buildedFn;

        HelperToken opBuilderInput;

        if (std::holds_alternative<HelperToken>(token))
        {
            opBuilderInput = std::get<HelperToken>(token);
        }
        else
        {
            auto expressionToken = std::get<ExpressionToken>(token);
            opBuilderInput = toBuilderInput(expressionToken);
        }

        auto op = baseHelperBuilder(opBuilderInput.name, opBuilderInput.targetField, opBuilderInput.args, buildCtx);

        if (op->isAnd())
        {
            std::vector<std::function<bool(base::Event)>> andOps;
            auto andOp = op->getPtr<base::And>();
            for (auto& ops : andOp->getOperands())
            {
                if (ops->isTerm())
                {
                    andOps.push_back(ops->getPtr<base::Term<base::EngineOp>>()->getFn());
                }
                else
                {
                    throw std::runtime_error("Check stage: Only 1 level of AND is supported.");
                }
            }
            buildedFn = [andOps](base::Event event) -> bool
            {
                for (auto& op : andOps)
                {
                    if (!op(event))
                    {
                        return false;
                    }
                }
                return true;
            };
        }
        else
        {
            buildedFn = op->getPtr<base::Term<base::EngineOp>>()->getFn();
        }

        return buildedFn;
    };
}

base::Expression checkExpressionBuilder(const std::string& logicExpr, const std::shared_ptr<const IBuildCtx>& buildCtx)
{
    // Apply definitions
    auto replacedExpr = buildCtx->definitions().replace(logicExpr);
    // TODO: make a factory and inject this dependency
    auto evaluator = logicexpr::buildDijstraEvaluator<base::Event, BuildToken>(
        replacedExpr, getTermBuilder(buildCtx), getTermParser());

    // Trace
    auto name = fmt::format("check: {}", logicExpr);
    const auto successTrace = fmt::format("[{}] -> Success", name);
    const auto failureTrace = fmt::format("[{}] -> Failure", name);

    // Return expression
    return base::Term<base::EngineOp>::create("stage.check",
                                              [=](base::Event event)
                                              {
                                                  if (evaluator(event))
                                                  {
                                                      return base::result::makeSuccess(event, successTrace);
                                                  }
                                                  else
                                                  {
                                                      return base::result::makeFailure(event, failureTrace);
                                                  }
                                              });
}

} // namespace

base::Expression checkBuilder(const json::Json& definition, const std::shared_ptr<const IBuildCtx>& buildCtx)
{
    if (definition.isArray())
    {
        return checkListBuilder(definition.getArray().value(), buildCtx);
    }
    else if (definition.isString())
    {
        return checkExpressionBuilder(definition.getString().value(), buildCtx);
    }
    else
    {
        throw std::runtime_error(fmt::format(
            "Stage '{}' expects an array or string but got '{}'", syntax::asset::CHECK_KEY, definition.typeName()));
    }
}

} // namespace builder::builders
