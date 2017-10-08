/* Raffaello Secchi - University of Aberdeen - Dec 2010 */

/* fullrun.c executes a simulation command that writes a statistic
   on standard output as many time as needed for confidence 
   intervals to converge. If the program outputs several statistic,
   fullrun.c tries to converge each one of them.
   
*/

/* Compile:   gcc fullrun.c -lpthread -lm  -lconfig           */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/wait.h>
#include <semaphore.h>
#include <math.h>
#include <libconfig.h>

#include <sys/time.h>
#include <sys/resource.h>


#define EPS   0.000001
#define min(a,b)   ((a)<(b))?(a):(b)
#define max(a,b)   ((a)>(b))?(a):(b)

// Defaults
#define MAX_OUTPUT	1024
#define DATA_BUF_SIZE	1024
#define NUM_JOBS          16
#define REFRESH           20
#define MAX_JOBS        1024
#define CONFIDENCE      0.99
#define PERCENTILE      0.90
#define MAX_ERR         0.05
#define MIN_RUNS          20
#define MAX_RUNS        2000 

char cfg_pathname[1024];
char conf_file[] = ".fullrun";

/* statistics */
int num_samples;
struct {
	int type;

	double estimate;
	double est_error;

	/* used for mean estimation */
	double sum_total;
	double sum_squares;

} stats[MAX_OUTPUT];

struct data_run {

	double data_val;

	/* tree enqueueing */
	struct data_run *lp;
	struct data_run *rp;
	int count;

};


struct data_run* wmp;
int c_index;
int cols;
int col_eval;
double c_value;
char conf_name[1024];


struct data_run* data_sort[MAX_OUTPUT];
int num_cycles;
int min_runs;
int num_jobs;
int refresh;
int end_run;
double max_err;
double confidence;
double perc;
int run_argc;
char** run_argv;
sem_t mutex;

double q_perc;
double perc, perc_var, perc_min;

void output_results()
{
int i;

	for(i=1; i<run_argc; i++)
		printf("%s ",run_argv[i]);


	for(i=0; i<cols; i++) 
		printf("%lf %lf ",
			stats[i].estimate, 
			stats[i].est_error
			 );
	
	printf("\n");

}

void sig_quit()
{
	output_results();
	exit(0);

}

void insert_sort(struct data_run** ep)
{
struct data_run *mp;

	mp = *ep;
	
	if ( mp == NULL ) {
		*ep = wmp;
		return;
	}

	if ( c_value < mp->data_val ) {
		mp->count++;
		insert_sort(&mp->lp);
		return;
	} 

	insert_sort(&mp->rp);

}

void print_list(struct data_run* list)
{

	if (!list)
		return;

	print_list(list->lp);
	printf("%lf\n",list->data_val);
	print_list(list->rp);
}

struct data_run* _get_list(struct data_run*p, int index)
{
	if ( index == p->count )
		return p;

	if ( index < p->count )
		return _get_list(p->lp, index);

	return _get_list(p->rp, index-p->count-1);

}

double get_list(struct data_run*p, int index)
{
	if ( index<0 )
		return _get_list(p, 0)->data_val;

	if ( index > num_samples-1 )
		return _get_list(p, num_samples-1)->data_val;

	return _get_list(p, index)->data_val;
}


double erfinv(double x)
{
	double xmax, xmin, xmid;

	if (x>1.0) {
		fprintf(stderr, "erfinv: argument not valid\n");
		exit(1);
	}
		
	if (x<0.0)
		return -erfinv(-x);

	xmin = 0.0;
	xmax = 1.0;

	while( erf(xmax) < x ) {
		xmin = xmax;
		xmax = xmin + 1.0;
	}

	while( xmax - xmin > EPS ) {
		xmid = (xmax + xmin) / 2;
		(erf(xmid)>x) ? (xmax = xmid) : (xmin = xmid);
	}
	

	return (xmax+xmin)/2;

}

/* Gaver-Kadafar t-student distribution approx */
/* It calculates the percentiles of t-distribution */
/* given z=normal percentile, n=degrees of freedom */
double t_distr(double z,int n)
{
double g;

	if ( n < 2 )
		return 0.0;

	g = ( n - 1.5 - 0.1/n + 0.5825/(n*n) ) / ( (n-1)*(n-1) );
	return sqrt(n*exp(z*z*g) - n);
	
}

void check_range_int(int* p, int a, int b, char* errorstr) 
{
	if ( a <= (*p) && (*p) <= b )
		return;

	fprintf(stderr, "%s: %s invalid\n", cfg_pathname, errorstr);
	exit(1);

}

void check_range_float(double* p, double a, double b, char* errorstr) 
{

	if ( a <= (*p) && (*p) <= b )
		return;

	fprintf(stderr, "%s: %s invalid\n", cfg_pathname, errorstr);
	exit(1);

}


void config_fullrun() 
{
config_t cfg, *cf;
config_setting_t* cfg_type;
char* type_name;
int cfg_type_len, i;
int work;

	/* init to defaults */	
	num_cycles = MAX_RUNS;
	confidence = CONFIDENCE;
	perc = PERCENTILE;
	max_err = MAX_ERR;
	num_jobs = NUM_JOBS;
	refresh = REFRESH;
	min_runs = MIN_RUNS;

	strcpy(conf_name, "defaults");

	if (!access(conf_file, R_OK)) {
		/*local fullrun exists */
		strcpy(cfg_pathname, getenv("PWD"));
		strcat(cfg_pathname, "/");
		strcat(cfg_pathname, conf_file);

	} else {

		/*seach in HOME */
		strcpy(cfg_pathname, getenv("HOME"));
		strcat(cfg_pathname, "/");
		strcat(cfg_pathname, conf_file);
		if (access(cfg_pathname, R_OK))
			return;
		
	}

	strcpy(conf_name, cfg_pathname);

	cf = &cfg;
	config_init(cf);

	if (!config_read_file(cf, cfg_pathname)) {
		fprintf(stderr, "using defaults\n");
		fprintf(stderr, "%s:%d - %s\n",
			cfg_pathname,
			config_error_line(cf),
			config_error_text(cf));

		exit(1);
	}


	cfg_type_len = 0;
	if ((cfg_type = config_lookup(cf,"type")))
		cfg_type_len = config_setting_length(cfg_type);

	for(i=0; i<cfg_type_len; i++) {
		type_name = (char*)config_setting_get_string_elem(cfg_type,i);
		switch( type_name[0] ) {

			case 'a':
				stats[i].type  = 0;
				break;
			
			case 'p':
				stats[i].type  = 1;
				break;

			default:
				fprintf(stderr, "type %d not recognised\n", i);
				exit(0);
		}
	}

	/* overriding defaults */

	/* FP vars */
	config_lookup_float(cf, "confidence", &confidence);
	config_lookup_float(cf, "percentile", &perc);
	config_lookup_float(cf, "max_err", &max_err);

	/* INT vars */
	config_lookup_int(cf, "num_jobs", &work); 
	num_jobs = (int)work;
	config_lookup_int(cf, "refresh", &work);
	refresh = (int)work;
	config_lookup_int(cf,"max_runs", &work);
	num_cycles = (int)work;
	config_lookup_int(cf, "min_runs", &work);
	min_runs = (int)work;
		
	check_range_int(&num_jobs, 1, 50 ,"num_jobs");
	check_range_int(&refresh, 1, 200 ,"refresh");
	check_range_int(&num_cycles, 1, 1000000 ,"num_runs");
	check_range_int(&min_runs, 1, 1000000 ,"min_runs");
	check_range_float(&confidence, 0, 1 ,"confidence");
	check_range_float(&perc, 0, 1 ,"percentile");
	check_range_float(&max_err, 0, 1 ,"max_err");

	config_destroy(cf);
}


void reset_stats()
{
int i;

	for(i=0; i<MAX_OUTPUT; i++) {

		stats[i].sum_total = 0.0;
		stats[i].sum_squares = 0.0;
		stats[i].estimate = 0.0;
		stats[i].est_error = 0.0;

	}

}


int update_stats()
{
int converged, i;
double mean, var, student_t, p_err, fmean, stder;
double delta, m_min, m_max, m_mid, perc_n;
int i_min, i_max;

	/* Now update the statistics */
	converged = 0;

	for(i=0; i<cols; i++) {

		if (stats[i].type == 0) {


			mean = stats[i].estimate = stats[i].sum_total/num_samples;
			var = (stats[i].sum_squares - num_samples*mean*mean)/(num_samples-1);
			student_t = t_distr(q_perc, num_samples-1);
			fmean = fabs(mean);
			stder = (var>0)? (student_t*sqrt(var/num_samples)):0;
			p_err = stats[i].est_error = (fabs(mean)==0) ? 0.0: stder/fmean;

			if ( p_err < max_err )
				converged++;

	
		} else
		if (stats[i].type == 1) {
			
			//printf("checking get_list ... %d \n", num_samples);
			//for(j=0; j<num_samples; j++) 
			//	printf("%lf\n",get_list(data_sort[i], j));
			//printf("comp\n");
			//print_list(data_sort[i]);

			/* binomial approximated by normal */
			if ( num_samples*perc_min > 5 ) 
			{

			
				delta = q_perc * sqrt(num_samples*perc_var);
				perc_n = num_samples*perc;
				// printf("num_samples=%d ", num_samples);
				// printf("q_perc=%lf perc_var=%lf ", q_perc, perc_var);
				// printf("delta=%lf perc_n=%lf perc=%lf ", delta, perc_n, perc);

				i_min = perc_n - delta - 1.5;
				i_max = perc_n + delta + 0.5;

				// printf("%d %d ", i_min, i_max);

				m_min = get_list(data_sort[i], i_min);
				m_max = get_list(data_sort[i], i_max);
	
				m_mid = stats[i].estimate = (m_max + m_min)/2;
				p_err = stats[i].est_error =
					(m_max - m_min)/(2*fabs(m_mid));

				// printf("m_min=%lf m_max=%lf m_mid=%lf p_err=%lf\n", m_min, m_max, m_mid, p_err);

				if ( p_err < max_err )
					converged++;
			}
		
		}
	}

	return converged;

}


/* Update stats vector (critical section) */
/* Exit if simulation converged or maximum runs reached */ 
void save_data(double* smp)
{

	sem_wait(&mutex);

	num_samples++;

	for(c_index=0; c_index<cols; c_index++) {

		c_value = smp[c_index];
		//printf("%lf\n", c_value);

		if (stats[c_index].type == 0) {

			stats[c_index].sum_total += c_value;
			stats[c_index].sum_squares += c_value*c_value;
	
		} else
		if (stats[c_index].type == 1) {


			/* inserting in queue */

			wmp = malloc(sizeof(struct data_run));
			wmp->rp = wmp->lp = NULL;
			wmp->data_val = c_value;
			wmp->count = 0;

			insert_sort(&data_sort[c_index]);
			// printf("new list\n");
			// print_list(data_sort[c_index]);
			
		}

	}

	if ( !(num_samples%refresh) ) {

		/* check termination condition */
		if ( (num_samples>min_runs) && 
			(update_stats() == cols || 
			num_samples > num_cycles)) 
		{
			// output_results();
			
			end_run = 0;
		}

	}

	sem_post(&mutex);
	return;

}


/* This thread schedules a simulation and collects
   results from standard output  */
void* sched_run(void* par) 
{
int p[2];
char data_buf[DATA_BUF_SIZE];
char *data_p;
int read_ret;
double drp[MAX_OUTPUT];

double data_scan;
int byte_scan, num_vals;



	while(end_run) {

		bzero(data_buf, DATA_BUF_SIZE);
		pipe(p);
		
		if (!vfork()) {
			signal(SIGINT, SIG_IGN);

			close(p[0]);
			dup2(p[1], STDOUT_FILENO);
			if (execvp(run_argv[0], run_argv)) 
			{
				sem_wait(&mutex);
				if ( end_run )
					fprintf(stderr, "fullrun: invalid command\n");
				end_run = 0; 
				sem_post(&mutex);
				exit(0);
			}
		}

		/* Reader */
		close(p[1]);

		data_p = data_buf;
		while((read_ret=read(p[0],data_p,1)
			&& (data_p-data_buf) < DATA_BUF_SIZE ) > 0)
			data_p ++;
		
		close(p[0]);
		wait(0);

		/* get values */
		num_vals = 0;
		data_p = data_buf;
		while(sscanf(data_p, "%lf%n", &data_scan, &byte_scan)>0) {
			drp[num_vals] = data_scan;
			data_p += byte_scan;
			num_vals++;
		}

		if (num_vals != cols) {
			sem_wait(&mutex);
			if (cols == -1) 
				cols = num_vals;
			
			if (cols == 0 || cols != num_vals) {
				if (end_run)
					fprintf(stderr, "fullrun: invalid number of outputs\n");
				sem_post(&mutex);
				exit(0);
			}
			sem_post(&mutex);
			//drp->num_vals = num_vals;
		}
		save_data(drp);

	}

	return NULL;
}



int main(int argc, char* argv[]) 
{
pthread_t pthread[MAX_JOBS];

int i;
	
	config_fullrun();

	if ( argc < 2) {
		fprintf(stderr, "\n%s: usage %s command <arg1> <arg2> ...\n",argv[0], argv[0]);
		fprintf(stderr, "\n");
		fprintf(stderr, "\tvariables must be configured in .fullrun\n");
		fprintf(stderr, "\tin local or home directory (format variable=value;)\n");
		fprintf(stderr, "\t\n");
		fprintf(stderr, "\tPress Ctrl-\\ to quit\n");
		fprintf(stderr, "\tPress Ctrl-C to see partial estimates\n");
		fprintf(stderr, "\t\n");
		fprintf(stderr, "\tcurrent configuration: %s\n", conf_name);
		fprintf(stderr, "\t\n");
		fprintf(stderr, "\tconfidence=%.5lf -  estimation confidence\n", confidence);
		fprintf(stderr, "\tpercentile=%.5lf -  target percentile estimation\n", perc);
		fprintf(stderr, "\tmax_err=%.5lf    -  maximum tolerable error\n", max_err);
		fprintf(stderr, "\tmin_runs=%-6d    -  minimum number of runs\n", min_runs);
		fprintf(stderr, "\tmax_runs=%-6d    -  maximum number of runs\n", num_cycles);
		fprintf(stderr, "\tnum_jobs=%-6d    -  number of parellel simulations\n", num_jobs);
		fprintf(stderr, "\trefresh=%-6d     -  number of runs before refreshing status\n", refresh);
		fprintf(stderr, "\n");
		exit(0);
	}

	reset_stats();

	cols = -1;
	end_run = 1;
	q_perc = M_SQRT2 * erfinv(confidence);	
	perc_var = perc*(1.0 - perc);
	perc_min = min(perc, 1.0-perc);
	
	sem_init(&mutex, 1, 1);

	run_argc = argc-1;
	run_argv = &argv[1];

	signal(SIGINT, output_results);
	signal(SIGQUIT, sig_quit);
	setpriority(PRIO_PROCESS, 0, -20);

	for(i=0; i<num_jobs; i++) 
		pthread_create( &pthread[i], NULL, sched_run, NULL);


	/* Waits for jobs to end */
	for(i=0; i<num_jobs; i++) 
		pthread_join( pthread[i], NULL );
	
	output_results();

	return 0;
}


