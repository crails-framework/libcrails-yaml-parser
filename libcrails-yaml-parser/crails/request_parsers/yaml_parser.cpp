#include "yaml_parser.hpp"
#include <crails/context.hpp>
#include <yaml-cpp/yaml.h>

using namespace std;
using namespace Crails;

void RequestYamlParser::operator()(Context& context, function<void(RequestParser::Status)> callback) const
{
  static const regex is_yaml("(application|text)/((x-)|(vnd\\.))?yaml", regex_constants::extended | regex_constants::optimize);
  const HttpRequest& request = context.connection->get_request();

  if (request.method() != HttpVerb::get && content_type_matches(request, is_yaml))
  {
     wait_for_body(context, [callback]()
     {
       callback(RequestParser::Stop);
     });
  }
  else
    callback(RequestParser::Continue);
}

static void load_yaml_node(YAML::Node node, Data output);

static void load_yaml_subtree(YAML::Node node, Data output)
{
  DataTree subtree;

  load_yaml_node(*it, subtree.as_data());
  output.push_back(subtree.as_data());
}

static void load_yaml_sequence(YAML::Node node, Data output)
{
  for (auto it = node.begin() ; it != node.end() ; ++it)
  {
    switch (it->Type())
    {
    case YAML::NodeType::Undefined:
    case YAML::NodeType::Null:
      break ;
    case YAML::NodeType::Map:
    case YAML::NodeType::Sequence:
      load_yaml_subtree(*it, output);
      break ;
    default:
      output.push_back(it->as<string>());
      break ;
    }
  }
}

static void load_yaml_map(YAML::Node node, Data output)
{
  for (auto it = node.begin() ; it != node.end() ; ++it)
  {
    const string key = it->first.as<string>();
    Data target(output);

    if (key != "<<") // alias support
      target = output[key];
    load_yaml_node(it->second, target);
  }
}

static void load_yaml_node(YAML::Node node, Data output)
{
  switch (node.Type())
  {
  case YAML::NodeType::Undefined:
  case YAML::NodeType::Null:
    break ;
  case YAML::NodeType::Sequence:
    load_yaml_sequence(node, output);
    break ;
  case YAML::NodeType::Map:
    load_yaml_map(node, output);
    break ;
  default:
    output = node.as<string>();
    break ;
  }
}

void RequestYamlParser::body_received(Context& context, const string& body) const
{
  YAML::Node yaml = YAML::Load(body);

  load_yaml_node(yaml, context.params.as_data());
}
