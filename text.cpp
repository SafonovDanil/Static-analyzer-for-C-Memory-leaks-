#include <iostream>

void bufSelect(int len, int **p, int *fastbuf){
    if(len > 10)
        *p = (int *)malloc(len);
    else
        *p = fastbuf;
}

int* test1() {
    return new int;
}

int* test2() {
    int *p = new int;
    return new int;
}

void test3(){
    int *p = new int;
    int **ptr = &p;
    delete *ptr;
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


    UPD         3
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


    //TODO обработку вызова функций нужно полностью переделать, она должна возвращать/не возвращать значение, + межпроцедурный анализ
    FUNC_CALL       5   //TODO +необходимо обрабатывать тип возвращаемого значения
    {
        "line": "func_name(b)",
        "type": 5,
        "left": "", //может не быть
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





    mstgObj ------------- old version
    {
        "mstgLeft":
        {
            "action": "Reg",  //toBase toGlobal
            "attr": var_name
        },
        "mstgRight":
        {
            "action": "toBase", //может отсутствовать
            "state": "Accessed"
        }
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














/*void bufSelect(int len, int **p, int *fastbuf){//INIT_LIST
                //len - VARIABLE, *p, p - EXTERNED
    if(len <= 10)   //BRANCH
        *p = fastbuf;   //UPD справа и слева - указатели, нужно занести в mstg
    else
        *p = (int *)malloc(len); //UPD справа - захват памяти, нужно занести в mstg
}

void foo1(){        //INIT_LIST список пустой
    int len = 16;   //INIT_VAR
                    //добавить в список ПЕРЕМЕННЫХ
    int *p;         //INIT_VAR
                    //добавить в список ПЕРЕМЕННЫХ

                    //ВАЖНО
                    //обрабатываю именно так, см. строки 66, 67 c_program.json
                    //тк удобнее рассматривать p, *p как разные переменные,
                    //а не переменные с именем p типов int и int*
                    //в частности, заносить в mstg, об этом позже

    int fastbuf[10];//INIT_VAR      //TODO определиться как это обработать
                    //добавить в список ПЕРЕМЕННЫХ
    bufSelect(len, &p, fastbuf);    //FUNC_CALL
                                    //создать список аргументов
}

void foo2(){
    int len = 8;
    int *p;
    int fastbuf[10];
    bufSelect(len, &p, fastbuf);
}

int main(){
    bool flag = true;   //INIT_VAR
                        //добавить в список ПЕРЕМЕННЫХ
                        // "left": "flag,
                        // "right": true

    if(flag)            //BRAMCH
                        //id true / flase
                        //добавить аннотацию к ребрам

        foo1();         //FUNC_CALL
                        //создать список аргументов
    else
        foo2();         //FUNC_CALL
                        //создать список аргументов
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


    UPD         3
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


    //TODO обработку вызова функций нужно полностью переделать, она должна возвращать/не возвращать значение, + межпроцедурный анализ
    FUNC_CALL       5   //TODO +необходимо обрабатывать тип возвращаемого значения
    {
        "line": "func_name(b)",
        "type": 5,
        "left": "", //может не быть
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





    mstgObj ------------- old version
    {
        "mstgLeft":
        {
            "action": "Reg",  //toBase toGlobal
            "attr": var_name
        },
        "mstgRight":
        {
            "action": "toBase", //может отсутствовать
            "state": "Accessed"
        }
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


