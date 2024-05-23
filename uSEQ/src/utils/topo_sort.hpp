#ifndef TOPO_SORT_H_
#define TOPO_SORT_H_
#include <algorithm>
#include <map>
#include <set>
#include <stack>
#include <vector>

template <typename T>
class DependencyGraph
{
public:
    void set_dependencies(const T& node, const std::set<T>& dependencies)
    {
        // TODO: convert to use sets instead
        std::vector<T> deps(dependencies.begin(), dependencies.end());

        graph[node] = deps;
        for (const T& dep : deps)
        {
            reverse_graph[dep].push_back(node);
            if (graph.count(dep) == 0)
            {
                graph[dep] = {}; // Insert node with no dependencies
            }
        }
    }

    std::vector<T> sort()
    {
        std::vector<T> sorted;
        std::set<T> visited;
        std::stack<T> stack;

        for (const auto& node : graph)
        {
            if (visited.count(node.first) == 0)
            {
                topological_sort_util(node.first, visited, stack);
            }
        }

        while (!stack.empty())
        {
            sorted.push_back(stack.top());
            stack.pop();
        }

        return sorted;
    }

private:
    void topological_sort_util(const T& node, std::set<T>& visited,
                               std::stack<T>& stack)
    {
        std::stack<T> temp_stack;
        temp_stack.push(node);

        while (!temp_stack.empty())
        {
            T top                         = temp_stack.top();
            bool all_dependencies_visited = true;

            for (const T& dep : reverse_graph[top])
            {
                if (visited.count(dep) == 0)
                {
                    temp_stack.push(dep);
                    all_dependencies_visited = false;
                    break;
                }
            }

            if (all_dependencies_visited)
            {
                visited.insert(top);
                stack.push(top);
                temp_stack.pop();
            }
        }
    }

    std::map<T, std::vector<T>> graph;
    std::map<T, std::vector<T>> reverse_graph;
};

#endif // TOPO_SORT_H_
