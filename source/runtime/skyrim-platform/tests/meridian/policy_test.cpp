#include "MeridianBrowserPolicy.h"
#include <iostream>

void Require(bool condition, const char* test)
{
  if (!condition) throw std::runtime_error(test);
}
int main()
{
  using namespace MeridianBrowserPolicy;
  Require(NormalizeUrl("file:///Data/Platform/UI/index.html?reload=1#chat") ==
    "mod://aetherius-client/index.html?reload=1#chat", "legacy translation");
  Require(NormalizeUrl(kPage) == kPage, "trusted entry point");
  for (auto url : { "https://example.com", "mod://other/index.html",
      "mod://aetherius-client/../index.html", "mod://aetherius-client/%69ndex.html",
      "file:///C:/secret.txt", "file:///Data/Platform/UI/index.html.evil",
      "mod://aetherius-client/index.html?x=\n", "mod://aetherius-client/index.html?x=\\",
      "mod://aetherius-client/index.html?x=\"" })
    Require(!NormalizeUrl(url), "unsafe URL accepted");
  ScriptQueue queue;
  Require(queue.Push(1, "first") && queue.Push(1, "second"), "enqueue");
  Require(queue.Pop(1) == "first" && queue.Pop(1) == "second", "FIFO");
  Require(!queue.Pop(1), "empty queue");
  queue.Push(1, "obsolete");
  queue.Reset(2);
  Require(!queue.Pop(1) && !queue.Pop(2) && !queue.Push(1, "stale"), "generation cancellation");
  for (std::size_t i = 0; i < kMaxOperations; ++i)
    Require(queue.Push(2, "x"), "count budget below limit");
  Require(!queue.Push(2, "overflow"), "count limit");
  queue.Reset(3);
  Require(queue.Push(3, std::string(kMaxBytes, 'x')), "byte budget at limit");
  Require(!queue.Push(3, "x"), "byte limit");
  Require(queue.Pop(3)->size() == kMaxBytes && queue.Push(3, "new"), "budget reclaimed");
  queue.Reset(4);
  Require(!queue.Push(4, std::string(kMaxBytes + 1, 'x')), "oversize script");
  std::cout << "PASS: trusted URL, unsafe URL rejection, FIFO, generation cancellation, count/byte budgets\n";
}
