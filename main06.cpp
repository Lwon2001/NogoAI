#include <iostream>
#include <string>
#include<cmath>
#include<cstdlib>
#include <cstring>
#include <ctime>
#include "jsoncpp/json.h"
using namespace std;


#define board_size 9  // 棋盘大小: 9 * 9
int Board[board_size][board_size];  // 棋盘
bool dfs_air_visit[board_size][board_size];  // 存储每个点是否被访问过
const int cx[]={-1,0,1,0};
const int cy[]={0,-1,0,1};
typedef pair<int,int> Action;  // 表示落子位置

bool inBoarder(int x, int y)
{
    return x>=0 && y>=0 && x < board_size && y < board_size;
}

//true: has air
bool hasAir(int fx, int fy)
{
    dfs_air_visit[fx][fy] = true;
    bool flag=false;
    for (int dir = 0; dir < 4; dir++)
    {
        int dx=fx+cx[dir], dy=fy+cy[dir];
        if (inBoarder(dx, dy))
        {
            if (Board[dx][dy] == 0)
                flag=true;
            if (Board[dx][dy] == Board[fx][fy] && !dfs_air_visit[dx][dy])
                if (hasAir(dx, dy))
                    flag=true;
        }
    }
    return flag;
}

//true: available
bool judgeAvailable(int board[board_size][board_size],int fx, int fy, int col)
{
    if (board[fx][fy]) return false;
    board[fx][fy] = col;
    memset(dfs_air_visit, 0, sizeof(dfs_air_visit));
    if (!hasAir(fx, fy))
    {
        board[fx][fy]  = 0;
        return false;
    }
    for (int dir = 0; dir < 4; dir++)
    {
        int dx=fx+cx[dir], dy=fy+cy[dir];
        if (inBoarder(dx, dy))
        {
            if (board[dx][dy] && !dfs_air_visit[dx][dy])
                if (!hasAir(dx, dy))
                {
                    board[fx][fy]  = 0;
                    return false;
                }
        }
    }
    board[fx][fy]  = 0;
    return true;
}

//---------策略部分---------

const double C = 1.4;  // 权重(UCB公式中的C)
int MyColor = 0;  // 我方棋子颜色
int ComputationBudget = 4;  // 限制向下搜索的次数

typedef struct Node  // 蒙特卡洛搜索树结点
{
    /* 棋盘属性 */
    int board[board_size][board_size];  // 棋盘
    int col;  // 该棋盘下一步的颜色(1为白，-1为黑)
    int round;  // 该棋盘下一步的回合数
    Action action;  // 该棋盘上一步的落子位置
    double value;  // 该状态的价值

    /* 结点属性 */
    int depth;  // 该结点的深度
    int visited_times;  // 该结点的搜索次数
    Node *parent;  // 父节点;
    Node *childs[board_size][board_size];  // 子节点指针数组,新建的孩子结点需要存在对应下标的位置
} Node;

Node *CantPutNode;


bool IsTerminal(Node *node)  // 判断是否为终止状态
{
    for (int i=0;i<9;i++)  // 检查棋盘是否能够继续落子，若能则不是终止状态
    {
        for (int j = 0; j < 9; j++)
        {
            if (judgeAvailable(node->board, i, j, node->col))
            {
                return false;
            }
        }
    }
    return true;
}

Node *PutChess(Node *node, Action action)  // 在结点node下棋,返回新结点指针(不管是否能下，交给调用的函数去维护：即Expand函数
{
    Node *new_node = new Node;
    /* 设置棋盘属性 */
    memcpy(new_node->board, node->board, sizeof(node->board));
    new_node->board[action.first][action.second] = node->col;  // 在棋盘上落子
    new_node->col = -node->col;  // 更换下棋方
    new_node->round = node->round + 1;  // 回合+1
    new_node->action = action;  // 表明上一步的动作

    /* 设置结点属性 */
    new_node->value = 0;  // 初始化价值
    new_node->depth = node->depth + 1;  // 深度+1
    new_node->visited_times = 0;  // 搜索次数初始为0
    new_node->parent = node;  // 设置父节点
    memset(node->childs, 0, sizeof(node->childs));
    node->childs[action.first][action.second] = new_node;
    return new_node;
}

// 在node结点下展开一个新结点,即随机选择一个可下子的空位置下子（需要确保与其他子节点不相同）,返回新结点的指针，若所有可扩展位置都扩展，则返回null
// （每个node维护一个childs_puts数组，每次随机选择一个空位置下子，若该位置已经有子，则重新选择）
Node*ExpandAll(Node *node)
{
    vector<int> available_list;  //  可扩展的子节点表
    for(int i = 0; i < board_size; i++)  // 构造可扩展的子节点表
    {
        for(int j = 0; j < board_size; j++)
        {
            if(!node->childs[i][j] && judgeAvailable(node->board, i, j, node->col))
            {
                available_list.push_back(i * 9 + j);
            }
        }
    }
    if(available_list.empty())
    {
        return nullptr;
    }
    srand(1);
    int random_position = rand() % available_list.size();
    int random_num = available_list[random_position];
    Action random_choice(random_num / 9, random_num % 9);  // 随机选取的结果
    return PutChess(node, random_choice);
}

bool IsAllExpand(Node *node)  // 判断是否所有子结点都已经展开,即该状态后的所有可能状态都在树中
{
    for(int i = 0; i < board_size; i++)  // 遍历所有可能存在的子节点
    {
        for(int j = 0; j < board_size; j++)
        {
            if(!node->childs[i][j])  // 子节点未进行扩展
            {
                if(judgeAvailable(node->board, i, j, node->col))  // 子节点未进行扩展且可落子，则表示未完全展开
                {
                    return false;
                }
            }
        }
    }
    return true;  // 如果所有可能的子结点中，要么是已经扩展，要么是无法进行落子，则表示所有子s结点都已经展开
}

// 返回结点node中UCB值最大的子结点，如果没有子结点，则返回NULL, 如果有多个UCB值相同的子结点，则返回任意一个
// status为1：表示为预测阶段的BestChild,即选择最大的UCB值的子结点
// status为0：表示为搜索阶段的BestChild,即选择最大的value的子结点
Node*BestUCBChild_ExpandALl(Node *node, int status)
{
    double max_ucb = -100;
    Node* best_child = nullptr;
    for(auto i = 0; i < board_size; i++)
    {
        for(auto j = 0; j < board_size; j++)
        {
            if(node->childs[i][j] && node->childs[i][j] != CantPutNode)
            {
                double temp_value = node->childs[i][j]->value;
                int temp_visited_times = node->childs[i][j]->visited_times;
                double temp_ucb;
                if(status == 1)
                {
                    temp_ucb = temp_value / temp_visited_times + C * sqrt(log(node->visited_times) / temp_visited_times);  //这个公式论文中还有优化方法
                }
                else
                {
                    temp_ucb = temp_value / temp_visited_times;
                }
                if(temp_ucb > max_ucb)
                {
                    max_ucb = temp_ucb;
                    best_child = node->childs[i][j];
                }
            }
        }
    }
    return best_child;
}

// MCTS的Selection和Expansion
//传入当前需要开始搜索的结点,根据exploration/exploitation算法选择最好的需要进行expand的结点
// 具体策略为:先找到从未被搜索过的结点,如果没有则找到UCB值最大的结点，如果UCB值相同则随机选择,选择后进行expand
Node*TreePolicy_ExpandAll(Node *node)
{
    while(!IsTerminal(node))
    {
        if(IsAllExpand(node))
        {
            node = BestUCBChild_ExpandALl(node, 1);
        }
        else
        {
            Node *child = ExpandAll(node);
            return child;
        }
    }
    return nullptr;  // 如果是叶节点，无法进行expand
}

double DefaultPolicy(Node *node)  //Simulation,返回对当前结点状态的评估值value，我方胜利,value为1,否则为0
{
    int temp_board[board_size][board_size];  // 保存node的棋盘
    memcpy(temp_board, node->board, sizeof(temp_board));
    int color = node->col;
    int rounds = node->round;
    while(!IsTerminal(node))  // 快速随机落子结束游戏
    {
        vector<int> available_list; //合法位置表
        for (int i=0;i<9;i++)  //随机选取可落子处落子
            for (int j=0;j<9;j++)
                if (judgeAvailable(node->board,i,j, color))
                    available_list.push_back(i * 9 + j);
        srand(1);
        int result = available_list[rand() % available_list.size()];  //随机选取
        node->board[result / 9][result % 9] = color;  //落子
        color = -color;  //交换下棋者颜色
        rounds++;  // 回合数增加
    }
    memcpy(node->board, temp_board, sizeof(temp_board));  // 复原node棋盘
    if(color != MyColor)
    {
        return 1;
    }
    else
    {
        return 0;
    }
}

void BackUp(Node *node, double value)  //BackPropagation
{
    while(node)
    {
        node->value += value;  // 价值增加
        node->visited_times++;  // 访问次数加1
        node = node->parent;  // BackPropagete
    }
}

Action MCTS(Node *node)
{
    for(int i  = 0; i < ComputationBudget; i++)
    {
        Node * expand_node = TreePolicy_ExpandAll(node);
        double value = DefaultPolicy(expand_node);
        BackUp(expand_node, value);
    }
    Node *BestNextNode = BestUCBChild_ExpandALl(node, 0);
    return BestNextNode->action;
}

int main()
{
    string str = "{\"requests\":[{\"x\":-1,\"y\":-1},{\"x\":7,\"y\":6},{\"x\":0,\"y\":0},{\"x\":0,\"y\":8},{\"x\":2,\"y\":2},{\"x\":7,\"y\":0},{\"x\":3,\"y\":1},{\"x\":6,\"y\":3},{\"x\":4,\"y\":1},{\"x\":5,\"y\":7},{\"x\":3,\"y\":7},{\"x\":4,\"y\":3},{\"x\":7,\"y\":7}],\"responses\":[{\"x\":2,\"y\":5},{\"x\":1,\"y\":0},{\"x\":1,\"y\":5},{\"x\":6,\"y\":0},{\"x\":7,\"y\":1},{\"x\":8,\"y\":6},{\"x\":8,\"y\":2},{\"x\":6,\"y\":6},{\"x\":4,\"y\":4},{\"x\":5,\"y\":3},{\"x\":3,\"y\":3},{\"x\":4,\"y\":5}]}";
    int x,y;
    // 读入JSON
//    getline(cin,str);
    //getline(cin, str);
    Json::Reader reader;
    Json::Value input;
    reader.parse(str, input);
    // 分析自己收到的输入和自己过往的输出，并恢复状态
    int turnID = input["responses"].size();
    for (int i = 0; i < turnID; i++)
    {
        x=input["requests"][i]["x"].asInt(), y=input["requests"][i]["y"].asInt();
        if(x!=-1) Board[x][y] = 1;
        x=input["responses"][i]["x"].asInt(), y=input["responses"][i]["y"].asInt();
        if (x!=-1) Board[x][y] = -1;
    }
    x=input["requests"][turnID]["x"].asInt(), y=input["requests"][turnID]["y"].asInt();
    if (x!=-1)
    {
        Board[x][y] = 1;
    }
    // 输出决策JSON
    Json::Value ret;
    Json::Value action;

    //以下为MCTS策略
    MyColor = -1;
    CantPutNode = new Node;  // 不能落子的特殊结点
    CantPutNode->visited_times = -1;  // 特殊结点的visited_times设为-1
    Node *root = new Node;  // 根节点
    memcpy(root->board, Board, sizeof(root->board));
    root->round = 1;
    root->action = Action(-1,-1);
    root->value = 0;
    root->col = MyColor;  // 我方永远视为黑色 -1

    root->parent = nullptr;
    root->depth = 0;
    root->visited_times = 0;
    memset(root->childs, 0, sizeof(root->childs));

    Action result = MCTS(root);
    action["x"] = result.first;
    action["y"] = result.second;
    ret["response"] = action;
    Json::FastWriter writer;

    cout << writer.write(ret) << endl;
    return 0;
}