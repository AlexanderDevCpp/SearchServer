#include "remove_duplicates.h"
using namespace std;

void RemoveDuplicates(SearchServer& search_server)
{
	map <set<string>,int> documents;
	set <int> for_remove;
	for (int id : search_server)
	{
		set<string> temp_words;
		for (auto& [word,freq] : search_server.GetWordFrequencies(id))
		{
			temp_words.insert(word);
		}
		if (documents.count(temp_words))
		{
			cout << "Found duplicate document id " << id << endl;
			for_remove.insert(id);
		}
		else
		{
			documents.emplace(temp_words, id);
		}
	}
	for (auto id : for_remove)
	{
		search_server.RemoveDocument(id);
	}
}