#include"json.hpp"
using json = nlohmann::json;

#include<iostream>
#include<vector>
#include<map>
#include<string>
using namespace std;

//json序列化： js.dump();
string func1(){
    json js;
    js["int"] = 2;
    js["string"] = "zhang san";
    js["array"] = {1,2,3,4};

    js["object"]["Li si"] = "China";
    js["object"]["Alice"] = "American"; 
    js["object"] = {{"Li si", "China"}, {"Alice", "American"}};

    vector<int> list = {1,2,5};
    map<int,string> map = {{1,"黄山"},{2,"华山"},{5,"庐山"}};
    js["list"] = list;
    js["mount"] = map;

    string jsonstr = js.dump();
    // cout<<jsonstr.c_str()<<endl;
    return jsonstr;
}


int main()
{
    string jsonstr = func1();
    //反序列化： json::parse(jsonstr)
    json js = json::parse(jsonstr);

    vector<int> v = js["list"];
    for(auto &i : v) cout<<i<<endl;
    cout<<endl;
    map<int,string> m = js["mount"];
    for(auto &itr : m) cout<<itr.first<<" "<<itr.second<<endl;
    cout<<endl;

    return 0;
}