#include <QCoreApplication>
#include <iostream>
#include <QMap>
#include <QJsonDocument>
#include <QJsonValue>
#include <QJsonObject>
#include <QJsonArray>
#include <QFile>
#include <QStack>
#include <QPair>
#include <QDir>
#include <QCoreApplication>

void saveJson(QJsonDocument document, QString fileName){
    QFile jsonFile(fileName);
    jsonFile.open(QFile::WriteOnly);
    jsonFile.write(document.toJson());
}

QJsonDocument loadJson(QString fileName)
{
    QFile jsonFile(fileName);
    jsonFile.open(QFile::ReadOnly);
    return QJsonDocument().fromJson(jsonFile.readAll());
}

enum LINE_TYPE{
    INIT_VAR = 1,
    INIT_LIST,
    ASSIGNMENT,
    BRANCH,
    FUNC_CALL,
    FREE,
    RETURN
};

enum VAR_TYPE{
    VARIABLE,
    HEAP,
    EXTERNED
};

QJsonArray intraprocedureAnalysis(QJsonArray& array){

    QMap<QString, QJsonValue> state_map;
    QMap<QString, QString> ptr_assignments;
    QMap<QString, QString> heaps_assignments;
    QStack<QString> externs;
    QList< QPair<QString, QJsonValue> > for_analysis;
    //QJsonArray summary;
    QJsonArray res;


    QStack<QString> discard;
    QStack<QString> retrain;


    for(int i = 0; !array[i].isUndefined(); i++){   //обход путей исполнения функции
        QJsonArray leak_list;

        QJsonValue tmp = array[i].toObject()["act"];

        for(int j = 0; !tmp[j].isUndefined(); j++){ //обход массива действий для одного пути
            QString var_name = tmp[j]["var_name"].toString();
            QString state = tmp[j]["state"].toString();
            QString attr = tmp[j]["attr_rt"].toString();

            if(tmp[j]["state"].toString() == "Allocated" || tmp[j]["state"].toString() == "Accessed"){  //обработка инициализации переменных
                externs.append(var_name);
                state_map.insert(var_name, tmp[j]);
            }

            if(tmp[j]["action"].toString() == "ToExtern"){  //обработка действия присвоения
                if(tmp[j]["memory_type"] == "Heap")
                    heaps_assignments.insert(var_name, tmp[j]["attr_rt"].toString());
                else
                    ptr_assignments.insert(tmp[j]["attr_rt"].toString(), var_name);
                if(state_map[tmp[j]["attr_rt"].toString()]["state"].toString() == "Accessed"){  //если предыдущее состояние - Accessed
                    QJsonObject leak_obj;
                    leak_obj["type"] = "Possible leak"; //возможная утечка
                    leak_obj["at"] = tmp[j];
                    leak_list.append(leak_obj);
                }

                    state_map[var_name] = tmp[j];
                for_analysis.append(QPair<QString, QJsonValue>(var_name, tmp[j]));  //действие является ключевым

                while(state_map.contains('*'+attr)){    //разименовывания указателя более не отслеживаются
                    state_map.remove('*'+attr);
                    attr = '*'+attr;
                }

                attr = tmp[j]["attr_rt"].toString();
                while(ptr_assignments.contains('*'+attr)){
                    ptr_assignments.remove('*'+attr);
                    attr = '*'+attr;
                }

                attr = tmp[j]["attr_rt"].toString();
                while(heaps_assignments.contains(attr)){    //если под этим указателем находился объект кучи
                    QString possible_leak = heaps_assignments[attr];
                    heaps_assignments.remove(attr);
                    attr = '*'+attr;
                    if(!heaps_assignments.keys().contains(possible_leak)){  //и этот объект кучи не отслеживает ни один указатель
                        QJsonObject mstg;
                        mstg["memory_type"] = "Heap";
                        mstg["var_name"]= possible_leak;
                        mstg["state"]= "Leaked";
                        state_map[possible_leak] = mstg;
                        QJsonObject leak_obj;
                        leak_obj["type"] = "Memory leak";   //утечка памяти
                        leak_obj["at"] = tmp[j];
                        leak_list.append(leak_obj);
                    }
                }
            }

            if(tmp[j]["action"].toString() == "ToAlloc"){   //действие захвата памяти
                state_map.insert(var_name, tmp[j]);
                for_analysis.append(QPair<QString, QJsonValue>(var_name, tmp[j]));
            }

            if(tmp[j]["action"].toString() == "ToReturn"){  //обработка оператора return
                state_map.insert(var_name, tmp[j]);
                for_analysis.append(QPair<QString, QJsonValue>(var_name, tmp[j]));
            }

            if(tmp[j]["action"].toString() == "ToFree"){    //обработка действия освобождения памяти
                for_analysis.append(QPair<QString, QJsonValue>(var_name, tmp[j]));
                if(heaps_assignments.contains(tmp[j][var_name].toString())){
                    QJsonObject mstg;
                    mstg["memory_type"] = "Var";
                    mstg["var_name"]= state_map[heaps_assignments[var_name]];
                    mstg["state"]= "Freed";
                    discard.append(heaps_assignments[var_name]);

                    state_map[heaps_assignments[var_name]] = mstg;
                    heaps_assignments.remove(var_name);

                }
                if(heaps_assignments.values().contains(ptr_assignments[var_name])){
                    QJsonObject mstg;
                    mstg["memory_type"] = "Extermed";
                    mstg["state"]= "Freed";
                    mstg["var_name"]= ptr_assignments[var_name];
                    state_map[ptr_assignments[var_name]] = mstg;

                    mstg["memory_type"] = "Heap";
                    QList<QString> keys = heaps_assignments.keys();
                    QList<QString> values = heaps_assignments.values();
                    QString key;
                    for(int ix = 0, n = keys.size(); ix < n; ix++){
                        if(values[ix] == ptr_assignments[var_name])
                            key = keys[ix];
                    }
                    state_map[key] = mstg;
                }
                //check ptr_assignments
            }

        }

        for(int i=0, n = state_map.size(); i < n; i++){ //анализ состояний памяти к конечному узлу в графе, производится в соотвествии с правилами, указанными в статье
            QString state = state_map.first()["state"].toString();
            if(state_map.first()["memory_type"].toString() == "Heap"){
                if(state == "Freed" || state == "Relinquished")
                    discard.append(state_map.first()["var_name"].toString());
                if(state == "Leaked"){
                    discard.append(state_map.first()["var_name"].toString());
                    //leak alarm
                }
                if(state == "Escaped"){
                    retrain.append(state_map.first()["var_name"].toString());
                }
            }
            else{
                if(state == "Freed")
                    retrain.append(state_map.first()["var_name"].toString());
                if(state == "Escaped"){
                    retrain.append(state_map.first()["var_name"].toString());
                }
            }
            auto str =  state_map.firstKey();
            state_map.remove(str);
        }
        QJsonObject copy = array[i].toObject();
        QJsonArray acts;
        for(int i = 0, n = for_analysis.size(); i < n; i++){    //анализ ключевых действий на утечки
            if(retrain.contains(for_analysis[i].first) && !discard.contains(for_analysis[i].first))
                acts.append(for_analysis[i].second);
        }
        while(!for_analysis.empty()){   //поиск неосвобожденной памяти
            if(for_analysis.first().second["memory_type"].toString() == "Heap" &&
                    !externs.contains(for_analysis.first().second["attr_rt"].toString()) &&
                    for_analysis.first().second["action"] != "ToReturn" &&
                    !discard.contains(for_analysis.first().second["attr_rt"].toString())){
                QJsonObject leak_obj;
                leak_obj["type"] = "Unfreed memory";
                leak_obj["at"] = for_analysis.first().second.toObject();
                leak_list.append(leak_obj);
            }
            for_analysis.removeFirst();
        }
        copy["act"] = acts;
        copy["leaks list"] = leak_list;
        res.append(copy);
    }
    return res;
}

void leakDetect(QString file_name){
    QJsonDocument json;
    json = loadJson(file_name);

    QJsonValue globalJsonObj;
    globalJsonObj = json.object(); //из исходного файла создается json объект

    QJsonObject resultJsonObj;
    QJsonArray funcArray;
    QJsonObject funcObj;


    int i = 0;
    int j,k;
    int heapObjCount = 1;


    while(!globalJsonObj["functions"][i].isUndefined())         //цикл по массиву функций
    {
        QJsonArray nodesArray;
        QJsonObject nodesObj;
        funcObj = globalJsonObj["functions"][i].toObject();
        j = 0;
        //QJsonArray var_array;
        QMap<QString, QString> var_type_map;    //контейнер, хранящий типы переменных
        QJsonArray init_list;

        while(!globalJsonObj["functions"][i]["nodes"][j].isUndefined())     //цикл по массиву узлов функции (линейных участков кода)
        {
            QJsonArray mstgArray;
            k = 0;
            nodesObj = globalJsonObj["functions"][i]["nodes"][j].toObject();
            while(!globalJsonObj["functions"][i]["nodes"][j]["lines"][k].isUndefined())     //цикл по строкам в узле
            {
                switch(globalJsonObj["functions"][i]["nodes"][j]["lines"][k]["type"].toInt())
                {
                    case(LINE_TYPE::INIT_VAR):  //инициализация переменной
                    {
                        QString var_type = globalJsonObj["functions"][i]["nodes"][j]["lines"][k]["var_type"].toString();
                        QString var_name = globalJsonObj["functions"][i]["nodes"][j]["lines"][k]["var_name"].toString();
                        //QJsonValue var_value;
                        QJsonObject var_object;

                        while(var_name.contains('*')){         //если есть * в названии, добавить все разименовывания в map значений и типов
                            var_type_map.insert(var_name, var_type);
                            var_name.remove(0, 1);
                            var_type = var_type + '*';
                        }
                        var_type_map.insert(var_name, var_type);
                        var_name = globalJsonObj["functions"][i]["nodes"][j]["lines"][k]["var_name"].toString();//восстановливаю имя и тип переменной
                        var_type = globalJsonObj["functions"][i]["nodes"][j]["lines"][k]["var_type"].toString();
                        if(!globalJsonObj["functions"][i]["nodes"][j]["lines"][k]["right"].isUndefined()){ //если есть правая часть
                            QString rt = globalJsonObj["functions"][i]["nodes"][j]["lines"][k]["right"].toString();
                            if(rt.contains("new") || rt.contains("realoc") || rt.contains("malloc")){           //если происходит захват памяти
                                QJsonObject mstgObj;
                                QString heapObjName = "HeapObj" + QString::number(heapObjCount);

                                mstgObj["memory_type"] = "Heap";
                                mstgObj["var_name"] = heapObjName;
                                mstgObj["action"] = "ToExtern";
                                mstgObj["state"] = "Escaped";
                                mstgObj["attr_rt"] = var_name;

                                heapObjCount++;
                                mstgArray.append(mstgObj);
                            }
                        }
                        break;
                    }
                    case(LINE_TYPE::INIT_LIST): // список инициализации аргументов функции
                    {
                        int m = 0;
                        while(!globalJsonObj["functions"][i]["nodes"][j]["lines"][k]["init_list"][m].isUndefined()){
                            QString var_type = globalJsonObj["functions"][i]["nodes"][j]["lines"][k]["init_list"][m]["var_type"].toString();
                            QString var_name = globalJsonObj["functions"][i]["nodes"][j]["lines"][k]["init_list"][m]["var_name"].toString();

                            QJsonObject arg_obj;
                            arg_obj["var_name"] = var_name;
                            arg_obj["var_type"] = var_type;
                            init_list.append(arg_obj);

                            if(var_name.contains('*')){                         //если левая часть - указатель
                                QJsonObject mstgObj;
                                var_type_map.insert(var_name, var_type);

                                while(var_name.contains('*')){                  //отслеживание всех разименовываний указателя
                                    var_name.remove(0, 1);
                                    var_type = '*' + var_type;
                                    mstgObj["memory_type"] = "Externed";
                                    mstgObj["var_name"] = var_name;
                                    mstgObj["attr_lt"] = "Reg";
                                    mstgObj["state"] = "Accessed";
                                    var_type_map.insert(var_name, var_type);
                                    mstgArray.append(mstgObj);
                                }
                            }
                            else{
                                var_type_map.insert(var_name, var_type);
                            }
                            m++;
                        }
                        break;
                    }
                    case(LINE_TYPE::ASSIGNMENT):    //опертор присваивания
                    {
                        QString lt = globalJsonObj["functions"][i]["nodes"][j]["lines"][k]["left"].toString();
                        QString rt = globalJsonObj["functions"][i]["nodes"][j]["lines"][k]["right"].toString();

                        if(rt.contains("new") || rt.contains("realoc") || rt.contains("malloc")){           //если происходит захват памяти
                            QJsonObject mstgObj;
                            QString heapObjName = "HeapObj" + QString::number(heapObjCount);

                            mstgObj["memory_type"] = "Heap";
                            mstgObj["var_name"] = heapObjName;
                            mstgObj["action"] = "ToExtern";
                            mstgObj["state"] = "Escaped";
                            mstgObj["attr_rt"] = lt;

                            heapObjCount++;
                            mstgArray.append(mstgObj);
                        }

                        if(var_type_map[lt].contains('*') && var_type_map[rt].contains('*')){   //если справа и слева - указатели
                            QJsonObject mstgObj;
                            mstgObj["memory_type"] = "Externed";
                            mstgObj["var_name"] = rt;
                            mstgObj["attr_lt"] = "Reg";
                            mstgObj["attr_rt"] = lt;
                            mstgObj["action"] = "ToExtern";
                            mstgObj["state"] = "Escaped";
                            mstgArray.append(mstgObj);
                        }
                        break;
                    }
                    case(LINE_TYPE::BRANCH):        //оператор ветвления, обрабатывается только if
                   {
                        QJsonObject edges;
                        edges["predicate"] = globalJsonObj["functions"][i]["nodes"][j]["lines"][k]["predicate"];
                        edges["true_node_id"] = globalJsonObj["functions"][i]["nodes"][j]["lines"][k]["true_node_id"];
                        edges["false_node_id"] = globalJsonObj["functions"][i]["nodes"][j]["lines"][k]["false_node_id"];
                        nodesObj["edges"] = edges;
                        break;
                   }

                    case(LINE_TYPE::FREE):      //оператор освобождения захваченной памяти
                    {
                        QJsonObject mstgObj;

                        QString rt= globalJsonObj["functions"][i]["nodes"][j]["lines"][k]["arg"].toString();
                        mstgObj["var_name"] = rt;
                        mstgObj["memory_type"] = "Externed";

                        if(rt.contains('*'))
                            mstgObj["attr_lt"] = "toBase";
                        else if(rt.contains('&'))
                            mstgObj["attr_lt"] = "toGlobal";
                        else
                            mstgObj["attr_lt"] = "Reg";
                        mstgObj["action"] = "ToFree";
                        mstgObj["state"] = "Freed";
                        mstgArray.append(mstgObj);
                        break;
                    }

                    case(LINE_TYPE::RETURN):        //опертор return
                    {
                        QString rt= globalJsonObj["functions"][i]["nodes"][j]["lines"][k]["arg"].toString();
                        QJsonObject mstgObj;
                        if(rt.contains("new") || rt.contains("realoc") || rt.contains("malloc")){
                            QString heapObjName = "HeapObj" + QString::number(heapObjCount);
                            mstgObj["memory_type"] = "Heap";
                            mstgObj["var_name"] = heapObjName;
                            mstgObj["attr_rt"] = "rv";
                            mstgObj["action"] = "ToReturn";
                            mstgObj["state"] = "Escaped";
                        }
                        else{
                            mstgObj["memory_type"] = "Externed";
                            mstgObj["var_name"] = rt;
                            mstgObj["attr_lt"] = "Reg";
                            mstgObj["attr_rt"] = "rv";
                            mstgObj["action"] = "ToReturn";
                            mstgObj["state"] = "Escaped";
                        }
                        mstgArray.append(mstgObj);
                        break;
                    }
                }
                k++;
            }
            nodesObj["mstg"] = mstgArray;

            QJsonObject edges; //обработка перехода к следующему узлу без условий
            edges["true_node_id"] = globalJsonObj["functions"][i]["nodes"][j]["next_node"];
            nodesObj["edges"] = edges;

            nodesArray.append(nodesObj);
            j++;
        }
        funcObj["nodes"] = nodesArray;
        funcObj["init_list"] = init_list;

        //обход графа в глубину для составления путей исполнения и их анализа

        QStack<QPair<int,int>> stack; //стек для обхода графа в глубину
        j = 0;

        int prev_node = -1;
        QString predicate = "";
        QJsonValue tmpFunc = funcObj["nodes"].toArray();

        QStack<int> leaf_nodes;
        QJsonArray tmpSummaryArray;

        QMap<int, QJsonArray> summary_map; //сводки о действиях узлов, хранятся в контейнере, тк объекты json с множеством вложений сложно редактировать

        QJsonArray startArray;
        startArray.append(QJsonObject ());
        summary_map.insert(-1, startArray);

        QList<int> path;

        bool flag = true;
        while(!tmpFunc[j]["edges"]["true_node_id"].isUndefined() || !stack.isEmpty() || flag){ //обход в глубину
            tmpSummaryArray = summary_map[prev_node];
            for(int ix = 0; !tmpSummaryArray[ix].isUndefined() ;ix++){
                QJsonObject tmp_ = tmpSummaryArray[ix].toObject();      //извлечение информации из контейнера о предыдущем узле
                QJsonArray predicate_ = tmp_["predicate"].toArray();
                QJsonArray act_ = tmp_["act"].toArray();

                for(int jx = 0; !tmpFunc[j]["mstg"][jx].isUndefined() ;jx++){   //добавление информации о текущем узле
                    act_.append(tmpFunc[j]["mstg"][jx]);
                }
                if(predicate != "")
                    predicate_.append(predicate);

                tmp_["act"] = act_;
                tmp_["predicate"] = predicate_;
                summary_map[j].append(tmp_);
            }

            if(!tmpFunc[j]["edges"]["true_node_id"].isUndefined()){     //если не листовой узел
                if(!tmpFunc[j]["edges"]["false_node_id"].isUndefined())     //если ветвление существует
                    stack.push(QPair<int,int>(tmpFunc[j]["edges"]["false_node_id"].toInt(), j));

                prev_node = j;      //запоминаем индекс предыдущего узла
                j = tmpFunc[j]["edges"]["true_node_id"].toInt();    //устанавливаем индекс следующего узла
                predicate = tmpFunc[prev_node]["edges"]["predicate"].toString();
            }
            else{       //если узел - листовой
                leaf_nodes.push(j);
                if(!stack.isEmpty()){   //если стек узлов по ветвям "false" не пуст
                    flag = true;
                    QPair<int,int> pair = stack.pop();
                    j = pair.first;
                    prev_node = pair.second;
                    predicate = '!' + tmpFunc[prev_node]["edges"]["predicate"].toString();
                }
                else
                    flag = false;
            }
        }

        QJsonArray final_summary;
        QJsonArray final_path;
        while(!leaf_nodes.isEmpty()){ //внутрипроцедурный анализ на утечки для всех путей исполнения
            QJsonArray leaf = summary_map[leaf_nodes.pop()];
            final_path.append(leaf);    //добавляем путь исполнения
            tmpSummaryArray = intraprocedureAnalysis(leaf);     //проверяем путь на возможные утечки
            final_summary.append(tmpSummaryArray);      //добавляем сводку о пути в массив сводок
        }
        // полученная информация добавлятся в json объект.
        funcObj["path"] = final_path;
        funcObj["summary"] = final_summary;
        funcArray.insert(i, funcObj);
        i++;    //переход к следующей функции
    }
    resultJsonObj["functions"] = funcArray; //в модифицированных объеках функций имеем граф состояний памяти, пути исполнения, ключевые действия, отчет об утечках.
    json.setObject(resultJsonObj);
    saveJson(json, "C:/Users/samiy/Documents/kursovaya/NIR_Safonov/test.json");
}


int main(){
        leakDetect("C:/Users/samiy/Documents/kursovaya/NIR_Safonov/c_program.json");
}




/* JSON LINE TYPES EXAMPLES

    INIT_VAR        1
    {
        "line": "int a = 1",
        "type": 1,
        "var_type" : "int",
        "var_name" : "a",
        "right" : 1             //chech !IsUndifined
    }
    {
        "line": "int *b",
        "type": 1,
        "var_type" : "int",
        "var_name" : "*b"
    }


    INIT_LIST       2
    {
        "line": "void func(int a, *b)"
        "type": 2,
        "init_list":[
        {
            "var_name": "a",
            "var_type": "int"
        },
        {
            "var_name": "*b",
            "var_type": "int"
        }
        ]
    }


    ASSIGNMENT         3
    {
        "line": "*b = a",
        "type": 3,
        "left" : "*b",
        "right": "a"
    }



    BRANCH      4
    {
        "line": "if(*b)",
        "type": 4,
        "predicate": "*b",
        "true_node_id": 1,
        "false_node_id": 2
    }



    FUNC_CALL       5
    {
        "line": "func_name(b)",
        "type": 5,
        "left": "",
        "right": "func_name",
        "arg_list" : [
        {
            "arg" : "b" //maybe "var_name" or VALUE (for example 10)
                        //need to check type of value
        }
        ]
    }

    FREE        6
    {
        "line" : "delete(b);"
        "type": 6,
        "arg" : "b"
    },
    {
        "line" : "free(b);"
        "type": 6,
        "arg" : "b"
    }



    RETURN      7
    {
        "line" : "return b",
        "type": 7,
        "arg" : "b"
    }


    mstgObj ------------- current version
    {
        "memory_type": Externed,  //heap, static
        "var_name": *p
        "action": "Reg",  //toBase toGlobal
        "attr": , //может быть пусто
        "state": "Accessed"
    }


    var_object
    {
        "var_name": ,
        "var_type": ,
        "var_value":
    }
*/

