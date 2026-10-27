# Project 3 Design Log

**Author:** Biruk Kebede  
**P2 code used:** Instructor reference  
**Tools used:** ChatGPT helped plan the work, check code, explain fixes in the implementation, troubleshoot compilation, and review this log.

## 1. Hash table

I implemented separate chaining with a vector of buckets where each buckeet was another vector containing `std::unique_ptr<Tool>` objects. In the scenario that multiple names map to the same bucket, lookup searches that bucket and compares the names. This approach made sense to me because insertion and lookup are straightforward and collisons don't require searching neighboring buckets.

The registry begins with 16 buckets, then, before adding a new tool, it rejects null pointers and duplicate names. The registry doubles its bucket count before inserting if the number of stored tools is equal to the number of buckets. By doing this, the load factor is kept at or below 1.0. Additionally, every existing tool is assigned a bucket using the new bucket count.

Using the hash routine that was supplied in Appendix A, I was able to process every character using XOR and multiplication. Character order does affect the result and the hash function ultimately returns its full value and the registry separately applies modulo to obtain the bucket index.

The scale test produced these results:

| Measurement | Result |
|---|---:|
| Registered tools | 10,000 |
| Bucket count | 16,384 |
| Load factor | 0.610352 |
| Longest chain using the supplied hash | 6 |
| Longest chain using `s[0] + s.size()` | 9,000 |

All 9,000 names from `tool_1000` through `tool_9999` end up together. The supplied hash distributes these names much more evenly.

Individual resizes take O(n) work due to the fact that existing entries must be redistributed. Doubling leaves room for many insertions before another resize. Insertion is amortized O(1) with well distributed hashes and bounded-length names because the total redistribution work grows linearly with the number of tools across a sequence of insertions.

## 2. Pointer stability

Each tool is owned by the registry through a `unique_ptr` and moving that pointer between buckets is how ownership is transferred without moving the actual tool object. Vectors can also reallocate their pointer elements while the allocated tools remain at their original addresses.

The scale test is responsible for saving the pointer returned for `tool_0` and checking it again after all 10,000 insertions have been performed. The default destructor also works because destroying the buckets destroys their owning pointers, effectively releasing the tools.




## 3. Calculator trace

The evaluator uses two vectors as stacks: one for values and one for operators. The following trace shows `2 * (3 + 4) - 5`. Stack contents are listed from bottom to top.

| Token | Action | Values | Operators |
|---|---|---|---|
| `2` | Push the number. | `2` | empty |
| `*` | Push the operator. | `2` | `*` |
| `(` | Push the opening parenthesis. | `2` | `* (` |
| `3` | Push the number. | `2 3` | `* (` |
| `+` | Push without crossing the parenthesis. | `2 3` | `* ( +` |
| `4` | Push the number. | `2 3 4` | `* ( +` |
| `)` | Calculate 3 + 4, then remove `(`. | `2 7` | `*` |
| `-` | Calculate 2 * 7, then push subtraction. | `14` | `-` |
| `5` | Push the number. | `14 5` | `-` |
| End | Calculate 14 - 5. | `9` | empty |

By applying an operator, the right operand is first popped and then the left. There are equal-precedence operators which are applied before pushing the next operator in order to preserve left-to-right evaluation. Long expressions and deeply nested parentheses without recursive calls are handled using loops.

## 4. Errors as values

Invalid paths or divisions by zero can result from ordinary tool requests. By returning an error value, the harness is able to report the problem to the model and still continue the conversation. This is done without terminating the program.

Exceptions are more appropriate for programming mistakes, such as requesting a string from a JSON value that holds a number. The provided dispatcher also catches exceptions as a final safeguard. My workspace and calculator functions return explanatory messages for rejected inputs.

## 5. What I would design differently

The main thing I would design differently is storing each tool's name alongside its owning pointer. The way I have it now, lookup and resizing call `name()` which returns a string by value. Instead, caching the name could avoid those temporary strings and make the key explicit. The main tradeoff with this being additional storage and responsibility for keeping that key consistent with the tool's name.