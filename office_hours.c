//office_hours.c
/*
##########################################################
## COP4610 – Principles of Operating Systems – Summer C 2019
## Prof. Jose F. Osorio
## Student: Jeffrey Hernandez – 3748674
## Project: Multithreaded Programming
## Specs:  
## Due Date: 07/8/2019 by 11:55pm
## Module Name: office_hours.c
##
## I Certify that this program code has been written by me
## and no part of it has been taken from any sources.
##########################################################
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <time.h>


#define NO_ERR 0
#define THREAD_ERR -1
#define INPUT_CNT 3
#define FALSE 0
#define TRUE 1

int validate_input( int, char* a[]);

//FUNCTION PROTOTYPES
pthread_t Professor();	//Returns Thread Id of Professor
pthread_t Student();	//Returns Thread Id of each Student Thread
void* ProfessorLoop();
void* StudentLoop( void* );
void AnswerStart();
void AnswerDone();
void EnterOffice();
void LeaveOffice();
void QuestionStart();
void QuestionDone();
int getCapacity();
int getCurrentId();
int getRemainingStudents();
int getUnansweredQuestion();
int getAnswerGiven();
void setCurrentId( int );
void setUnansweredQuestion();

//SYNCHRONIZATION OBJECTS
pthread_mutex_t capacity_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t students_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t id_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t qa_mutex = PTHREAD_MUTEX_INITIALIZER;
	
//SHARED VARIABLES
int current_capacity = 0;
int remaining_students = 0;
int current_id = -1;
int capacity = -1;
int unanswered_question = 0;		//covered by qa_mutex
int answer_given = 0;				//covered by qa_mutex

int main(int argc, char* argv[])
{
	int num_students = -1;		//To hold arg1
	int i = 0;					//Loop index
	pthread_t professor_thread;	//Professor Thread
	pthread_t* thread_ids;		//Array of Student Threads

	//VALIDATE INPUT
	if ( !validate_input( argc, argv ) )
	{
		;
	}
	else
	{
		return THREAD_ERR;
	}
	
	//CAPTURE INPUT
	num_students = atoi(argv[1]);
	capacity = atoi(argv[2]);
	
	remaining_students = num_students;
	current_capacity = capacity;
	
	//ALLOCATE MEMORY TO STORE THREAD IDS
	thread_ids = malloc( sizeof( pthread_t ) * num_students );
	
	printf("Number of students with questions: %d\n", num_students);
	printf("Office capacity: %d\n\n", capacity);
	
	//CREATE PROFESSOR THREAD
	professor_thread = Professor();
	
	//CREATE STUDENT THREADS
	for ( i = 0; i < num_students; i++ )
	{
		thread_ids[i] = Student(i);
		//printf("Master Process Logs student thread id: %lu\n", thread_ids[i]);
	}
	
	//JOIN STUDENT THREADS
	for ( i = 0; i < num_students; i++ )
	{
		pthread_join( thread_ids[i], NULL);
		//printf("Master Process Joins thread[%d] with id: %lu\n", i, thread_ids[i]);
	}
	
	//FREE ALLOCATED MEMORY
	free( thread_ids );
	
	//JOIN PROFESSOR THREAD
	pthread_join( professor_thread, NULL );
	
	//printf("Master Process Joins Professor with id: %lu\n", professor_thread );
	
	return NO_ERR;
}

//STUDENT THREAD FUNCTIONS
pthread_t Student( int student_number )
{
	pthread_t student_thread;
	
	/*
	student_number may quickly be changed before it is dereferenced by a thread
	which my lead to inconsistent values. So a piece of memory is allocated
	for the value in student_number and the address of this memory is passed
	to the thread function for private use where it is also freed by the thread
	*/
	int* parameter = malloc( sizeof( int ) * 1 );
	*parameter = student_number;
	
	pthread_create( &student_thread, NULL, StudentLoop, parameter );
	
	//printf("           Student[%d] spawned with id: %lu\n", *parameter, student_thread);
	
	return student_thread;
}


void *StudentLoop( void* parameter )
{
	int student_number = *(int*)parameter;
	int num_questions = ( student_number % 4 ) + 1;
	int remaining_questions = num_questions;
	int current_question = 1;
	int in_office = 0;
	int i = 0;
	
	//ENTER THE OFFICE
	while ( !in_office )
	{
		pthread_mutex_lock( &capacity_mutex );
		
		if ( current_capacity != 0 )
		{
			printf("          Student[%d] enters the office\n", student_number);
			in_office = 1;
			EnterOffice();
			pthread_mutex_unlock( &capacity_mutex );
		}
		else
		{
			pthread_mutex_unlock( &capacity_mutex );
			usleep(10000);
			//printf("           Student[%d] waits outside of office\n", student_number);
		}
	}
	
	//EXHAUST QUESTIONS
	while ( remaining_questions > 0 )
	{
		pthread_mutex_lock( &qa_mutex );
		
		if ( unanswered_question )
		{
			pthread_mutex_unlock( &qa_mutex );	//WAIT YOUR TURN
		}
		else
		{
			printf("          Student[%d] asks a question (%d/%d)\n", student_number, current_question, num_questions );
			
			setCurrentId( student_number );
			
			QuestionStart();
			
			pthread_mutex_unlock( &qa_mutex );
			
			//WAIT FOR QUESTION TO BE ANSWERED
			while ( getAnswerGiven() == 0 )
			{
				usleep(1000);
			}
			
			QuestionDone();
			
			printf("          Student[%d] is satisfied\n", student_number );
			
			current_question++;
			remaining_questions--;
		}
	
	}
	
	printf("          Student[%d] leaves the office\n", student_number);
	
	LeaveOffice();
	
	free( parameter );  
}


//EnterOffice() is only called after locking the capacity_mutex
void EnterOffice()
{
	current_capacity--;
}

void LeaveOffice()
{
	//INCREASE OFFICE CAPACITY
	pthread_mutex_lock( &capacity_mutex );
	
	current_capacity++;
	
	pthread_mutex_unlock( &capacity_mutex );
	
	
	//DECREASE REMAINING STUDENTS
	pthread_mutex_lock( &students_mutex );
	
	remaining_students--;
	
	pthread_mutex_unlock( &students_mutex );
}

void QuestionStart()
{
	if ( unanswered_question )
	{
		printf("ERROR: STUDENT IS ATTEMPTING TO ASK A QUESTION OUT OF TURN!!\n");
	}
	else
	{
		unanswered_question = 1;
	}
}

void QuestionDone()
{
	pthread_mutex_lock( &qa_mutex );
	
	unanswered_question = 0;
	
	answer_given = 0;
	
	pthread_mutex_unlock( &qa_mutex );
}


//PROFESSOR THREAD FUNCTIONS
pthread_t Professor()
{
	static pthread_t professor_thread;
	
	pthread_create( &professor_thread, NULL, ProfessorLoop, NULL );
	
	//printf("Master Process spawning Professor Thread: %lu\n", professor_thread);
	
	return professor_thread;
}


void *ProfessorLoop()
{
	AnswerStart();
}

void AnswerStart()
{
	//BLOCKS WHEN THERE ARE NO STUDENTS IN THE ROOM
	while ( getRemainingStudents() > 0 )
	{
		if ( getCapacity() == capacity )
		{
			usleep(1000);	
		}
		else
		{
			while ( getCapacity() != capacity )
			{
	
				if ( getUnansweredQuestion() == 1 )
				{
					if ( getAnswerGiven() == 1 )
					{
						usleep(1000);
						
					}
					else
					{
						printf("Professor starts to answer the question of student %d\n", getCurrentId() );
						AnswerDone();
					}
				}
				else
				{
					//printf("Professor: Waiting for question to be asked...");
					usleep(1000);
				}
				
			}	
		}
	}
	printf("\nProfessor thread has answered all questions! Office hours have ended.\n");
}

void AnswerDone()
{
	printf("Professor is done with answer for student %d\n", getCurrentId() );
	
	pthread_mutex_lock( &qa_mutex );
	
	answer_given = 1;
	
	pthread_mutex_unlock( &qa_mutex );
}

//GETTER AND SETTER METHODS
int getCapacity()
{
	int val;

	pthread_mutex_lock( &capacity_mutex );
	
	val = current_capacity;
	
	pthread_mutex_unlock( &capacity_mutex );
	
	return val;
}

int getRemainingStudents()
{
	int val;
	
	pthread_mutex_lock( &students_mutex );
	
	val = remaining_students;
	
	pthread_mutex_unlock( &students_mutex );
	
	return val;
}

int getUnansweredQuestion()
{
	int val;
	
	pthread_mutex_lock( &qa_mutex );
	
	val = unanswered_question;
	
	pthread_mutex_unlock( &qa_mutex );
	
	return val;
}

int getAnswerGiven()
{
	int val;

	pthread_mutex_lock( &qa_mutex );
	
	val = answer_given;
	
	pthread_mutex_unlock( &qa_mutex );
	
	return val;
}

void setUnansweredQuestion()
{
	pthread_mutex_lock( &qa_mutex );

	if ( unanswered_question == 0 )
	{
		unanswered_question = 1;
	}
	else
	{
		unanswered_question = 0;
	}
	
	pthread_mutex_unlock( &qa_mutex );
}

int getCurrentId()
{
	int val;
	
	pthread_mutex_lock( &id_mutex );
	
	val = current_id;
	
	pthread_mutex_unlock( &id_mutex );
	
	return val;
}

void setCurrentId( int id )
{
	pthread_mutex_lock( &id_mutex );
	
	current_id = id;
	
	pthread_mutex_unlock( &id_mutex );
}

//INPUT VALIDATION
int validate_input(int argc, char* argv[])
{
	if ( argc <  INPUT_CNT )
	{
		printf("ERROR: User must specify number of students AND office capacity\n");
	
		return THREAD_ERR;
	}
	
	if ( atoi(argv[1]) < 1 )
	{
		printf("ERROR: Number of students must be 1 or greater\n");
		
		return THREAD_ERR;
	}
	
	if ( atoi(argv[2]) < 1 )
	{
		printf("ERROR: Capacity of office must be 1 or greater\n");
		
		return THREAD_ERR;
	}
	
	return NO_ERR;
}
