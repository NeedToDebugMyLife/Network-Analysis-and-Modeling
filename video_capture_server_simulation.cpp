#include <random>
#include <vector>
#include <string>
#include <cfloat>       //  DBL_MAX
#include <climits>      //  INT_MAX
#include <fstream>
#include <iostream>
using namespace std;

enum field_type
{
    T = 0,
    B = 1
};

enum server_status
{
    BUSY = 0,
    IDLE = 1
};

struct field
{
    int number;         //  serial number of the field
    field_type type;    //  field type (T: top field / B: bottom field)
    double complexity;  //  complexity of the field (fobs)
};

struct buffer
{
    int max_size;
    vector<field*> buff;
};

field_type TB;
field *F_en, *F_st, *F_tmp, *F_top, *F_bottom;
buffer *B_en, *B_st;
server_status S_en, S_st;

ofstream result_file;           // result file

bool last_discard;
int N_en, C_en, C_st, discard_counter, field_counter, next_event;
double mean_inter, mean_complex;
double alpha, sim_t, en_exe_t, en_exe_start_t, st_exe_t, st_exe_start_t;
double event_t[7];              //  event_t[0]: The time when the simulation ends (=28800.0)
                                //  event_t[1]: The time when the next field arrives at the buffer of the encoder
                                //  event_t[2]: The time when the next field enter the encoder
                                //  event_t[3]: The time when the next field leave the encoder
                                //  event_t[4]: The time when the next field arrives at the buffer of the storage server
                                //  event_t[5]: The time when the next field enter the storage server
                                //  event_t[6]: The time when the next field leave the storage server

random_device rd;
mt19937 gen(rd());
exponential_distribution<> inter;
exponential_distribution<> comp;

void init(int n, double mean_inter, double mean_complex);
void timing();
void arrive_en();
void enter_en();
void depart_en();
void arrive_st();
void enter_st();
void depart_st();
void report();

int main()
{
    result_file.open("simulation_results.csv");
    result_file << "MeanInter,MeanComplex,BufferSize,DiscardRate,Util_Encoder,Util_Storage" << endl;

    for (int i = 1; i <= 2; i++)
    {
        for (int j = 1; j <= 2; j++)
        {
            mean_inter = (1.0/120.0) / i;       //  mean of inter-arrival time
            mean_complex = 200.0 * j;           //  mean of field complexity

            for (int size_buff = 20; size_buff <= 100; size_buff += 20)
            {                
                init(size_buff, mean_inter, mean_complex);

                do
                {
                    timing();

                    switch(next_event)
                    {
                        case 0:
                            report();
                            break;
                        case 1:
                            arrive_en();
                            break;
                        case 2:
                            enter_en();
                            break;
                        case 3:
                            depart_en();
                            break;
                        case 4:
                            arrive_st();
                            break;
                        case 5:
                            enter_st();
                            break;
                        case 6:
                            depart_st();
                            break;
                    }
                } while (next_event != 0);
            }
        }
    }

    result_file.close();

    return 0;
}

void init(int n, double mean_inter, double mean_complex)
{
    inter = exponential_distribution<> (1.0 / mean_inter);
    comp = exponential_distribution<> (1.0 / mean_complex);

    B_en = new buffer;
    B_st = new buffer;

    TB = T;                     //  type of field

    alpha = 0.1;                //  fobs to byte ratio

    sim_t = 0.0;                //  current simulation time

    en_exe_t = 0.0;             //  total encoder execution time
    st_exe_t = 0.0;             //  total storage server execution time
    en_exe_start_t = DBL_MAX;   //  the time that encoder starts execution 
    st_exe_start_t = DBL_MAX;   //  the time that storage server starts execution 

    field_counter = 0;          //  counter of fields
    discard_counter = 0;        //  counter of discarded fields

    N_en = n;                   //  encoder buffer size
    C_en = 15800;               //  encoder capacity (fobs/s)
    C_st = 1600;                //  storage server capacity (bytes/s)

    S_en = IDLE;                //  encoder status
    S_st = IDLE;                //  storage server status

    B_en->max_size = 20;        //  encoder buffer size
    B_st->max_size = INT_MAX;   //  storage server buffer size

    last_discard = false;       //  last field discarded status (true if the last field is discarded)

    event_t[0] = 28800.0;
    event_t[1] = inter(gen);
    event_t[2] = DBL_MAX;
    event_t[3] = DBL_MAX;
    event_t[4] = DBL_MAX;
    event_t[5] = DBL_MAX;
    event_t[6] = DBL_MAX;
}

void timing()
{
    next_event = 0;
    double min_t = event_t[0];

    for(int i = 1; i < 7; i++)
    {
        if(event_t[i] < min_t)
        {
            next_event = i;
            min_t = event_t[i];
        }
    }

    sim_t = min_t;
}

int c = 0;
void arrive_en()
{
    //  insert field to the buffer of encoder
    field *f = new field;
    f->type = TB;
    f->number = field_counter;
    f->complexity = comp(gen);

    if(B_en->buff.size() == B_en->max_size)
    {   
        //  remove top field if current is bottom and top is not discarded
        //  only discard if (1) current is top 
        //                  (2) current is bottom and top is discarded
        if(f->type == B && !last_discard)
        {
            field *t = B_en->buff.back();
            B_en->buff.pop_back();
        
            // string s = (t->type == T) ? "T" : "B";
            // cout << "Field-" << t->number << "(" << s << ") is discarded at simulation time " << sim_t << endl;
    
            discard_counter++;
            delete t;
        }

        // string s = (f->type == T) ? "T" : "B";
        // cout << "Field-" << f->number << "(" << s << ") is discarded at simulation time " << sim_t << endl;
        
        discard_counter++;
        last_discard = true;
        delete f;
    }
    else if(f->type == B && last_discard)
    {
        // string s = (f->type == T) ? "T" : "B";
        // cout << "Field-" << f->number << "(" << s << ") is discarded at simulation time " << sim_t << endl;

        discard_counter++;
        last_discard = true;
        delete f;
    }
    else
    {
        last_discard = false;
        B_en->buff.push_back(f);
    }

    TB = (TB == B) ? T : B;
    field_counter++;

    //  schedule next event (Arrive_en)
    event_t[1] = sim_t + inter(gen);
    
    //  schedule next event (Enter_en)
    if(S_en == IDLE && !B_en->buff.empty())
        event_t[2] = sim_t;
}

void enter_en()
{
    //  move the first field into encoder
    F_en = B_en->buff[0];
    B_en->buff.erase(B_en->buff.begin());

    //  set encoder status busy
    S_en = BUSY;

    //  record the execution start time
    if(sim_t < en_exe_start_t)
        en_exe_start_t = sim_t;

    //  schedule next event (Enter_en)
    event_t[2] = DBL_MAX;

    //  schedule next event (Depart_en)
    double en_t = F_en->complexity / C_en;
    event_t[3] = sim_t + en_t;
}

void depart_en()
{
    //  remove the field from encoder
    F_tmp = F_en;

    //  set encoder status idle if no field is in buffer
    S_en = (B_en->buff.size() > 0) ? BUSY : IDLE;

    //  record execution period
    if(S_en == IDLE)
    {
        en_exe_t += (sim_t - en_exe_start_t);
        en_exe_start_t = DBL_MAX; 
    }

    //  schedule next event (Enter_en)
    event_t[2] = (B_en->buff.size() > 0) ? sim_t : DBL_MAX;

    //  schedule next event (Depart_en)
    event_t[3] =  DBL_MAX;

    //  schedule next event (Arrive_st)
    event_t[4] = sim_t;
}

void arrive_st()
{
    //  insert field to the buffer of storage server
    B_st->buff.push_back(F_tmp);

    //  schedule next event (Arrive_st)
    event_t[4] = DBL_MAX;

    //  schedule next event (Enter_st)
    if (B_st->buff.size() >= 2 && S_st == IDLE)
        event_t[5] = sim_t;
}

void enter_st()
{
    //  move the first two fields into storage server
    F_top = B_st->buff[0];
    B_st->buff.erase(B_st->buff.begin());

    F_bottom = B_st->buff[0];
    B_st->buff.erase(B_st->buff.begin());

    //  set storage server status busy
    S_st = BUSY;

    //  record the execution start time
    if(sim_t < st_exe_start_t)
        st_exe_start_t = sim_t;

    //  schedule next event (Enter_st)
    event_t[5] = DBL_MAX;

    //  schedule next event (Depart_st)
    double st_t = 0.1 * (F_top->complexity + F_bottom->complexity) / C_st;
    event_t[6] = sim_t + st_t;
}

void depart_st()
{
    delete F_top;
    delete F_bottom;

    //  set storage server status idle if the number of fields in buffer is less than 2
    S_st = (B_st->buff.size() >= 2) ? BUSY : IDLE;

    //  record execution period
    if(S_st == IDLE)
    {
        st_exe_t += (sim_t - st_exe_start_t);
        st_exe_start_t = DBL_MAX; 
    }
    
    //  schedule next event (Enter_st)
    event_t[5] = (B_st->buff.size() >= 2) ? sim_t : DBL_MAX;

    //  schedule next event (Depart_st)
    event_t[6] =  DBL_MAX;
}

int progress = 1;
int sim_counter = 1;

void report()
{
    if(S_en == BUSY)
        en_exe_t += (sim_t - en_exe_start_t);

    if(S_st == BUSY)
        st_exe_t += (sim_t - st_exe_start_t);

    double f = (double)discard_counter / (double)field_counter;
    double u_en = en_exe_t / sim_t;
    double u_st = st_exe_t / sim_t;

    cout << "Simulation " << sim_counter++ << " | ";
    cout << "Progress: ";
    for (int i = 0; i < 20; i++)
    {
        if(i < progress)
            cout << "▂";
        else
            cout << "▁▁";
    }
    cout << " " << progress*5 << "%" << endl;
    progress++;

    cout << "( Mean of inter-arrival time: " << mean_inter;
    cout << " / Mean of complexity: " << mean_complex;
    cout << " / Buffer size: " << N_en << " )" << endl;

    cout << "Frame discarded rate: " << f << endl;
    cout << "Utilization of encoder: " << u_en << endl;
    cout << "Utilization of storage server: " << u_st << endl;

    cout << endl << "======================================================================================" << endl << endl;

    result_file << mean_inter << "," 
                << mean_complex << "," 
                << N_en << "," 
                << f << "," 
                << u_en << "," 
                << u_st << endl;
}