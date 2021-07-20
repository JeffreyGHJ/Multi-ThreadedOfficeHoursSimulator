//omp_office.c
/*
##########################################################
## COP4610 – Principles of Operating Systems – Summer C 2019
## Prof. Jose F. Osorio
## Student: Jeffrey Hernandez – 3748674
## Project: Multithreaded Programming
## Specs:  
## Due Date: 07/8/2019 by 11:55pm
## Module Name: omp_office.c
##
## I Certify that this program code has been written by me
## and no part of it has been taken from any sources.
##########################################################
*/
//REQUIRED LIBRARIES
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <omp.h>
#include <time.h>

//MACROS
#define NO_ERR 0
#define THREAD_ERR -1
#define INPUT_CNT 3
#define FALSE 0
#define TRUE 1

//FUNCTION PROTOTYPES
int validate_input( int, char* a[]);
void ProfessorLoop();
void StudentLoop( int );
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
void setLock( omp_lock_t* );
void unsetLock( omp_lock_t* );

//SYNCHRONIZATION OBJECTS
omp_lock_t capacity_lock;
omp_lock_t students_lock;
omp_lock_t id_lock;
omp_lock_t qa_lock;
	
//SHARED VARIABLES
int capacity = -1;					//covered by capacity_lock
int current_capacity = 0;			//covered by capacity_lock
int remaining_students = 0;			//covered by students_lock
int current_id = -1;				//covered by id_lock
int unanswered_question = 0;		//covered by qa_lock
int answer_given = 0;				//covered by qa_lock

int main(int argc, char* argv[])
{
	int nthreads, tid;
	int num_students = -1;		//To hold arg1
	int i = 0;					//Loop index
	double start, end;
	
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
	
	//INITIALIZE OMP OBJECTS
	omp_set_num_threads( num_students + 1 ); 	/* +1 for professor thread */
	
	omp_init_lock( &capacity_lock );
	omp_init_lock( &students_lock );
	omp_init_lock( &id_lock );
	omp_init_lock( &qa_lock );
	
	printf("Number of students with questions: %d\n", num_students);
	printf("Office capacity: %d\n\n", capacity);
	
	start = omp_get_wtime();
	
	//OMP SPAWN THREADS AND BRANCH
	#pragma omp parallel shared( capacity, current_capacity, remaining_students, current_id, unanswered_question, answer_given ) \
				 		 private( nthreads, tid )
	{
		tid = omp_get_thread_num();

		if (tid == 0) 
		{
			nthreads = omp_get_num_threads();

			//printf("Number of threads = %d; Professor Threads (1) / Student Threads (%d)\n", nthreads, nthreads - 1);
			ProfessorLoop();
		}
		else
		{
			//printf("Student[%d] starting...\n",tid);
			StudentLoop( tid - 1 ); /* -1 so that the first student can have id 0 */
		}
	}
	
	end = omp_get_wtime();
	
	printf("Execution time: %f sec\n", end - start );
	
	return NO_ERR;
}

void StudentLoop( int id )
{
	int student_number = id;
	int num_questions = ( student_number % 4 ) + 1;
	int remaining_questions = num_questions;
	int current_question = 1;
	int in_office = 0;
	int i = 0;
	
	//ENTER THE OFFICE
	while ( !in_office )
	{
		setLock( &capacity_lock );
		
		if ( current_capacity != 0 )
		{
			printf("          Student[%d] enters the office\n", student_number);
			in_office = 1;
			EnterOffice();
			
			unsetLock( &capacity_lock );
		}
		else
		{
			unsetLock( &capacity_lock );

			usleep(10000);
			//printf("           Student[%d] waits outside of office\n", student_number);
		}
	}
	
	//EXHAUST QUESTIONS
	while ( remaining_questions > 0 )
	{
		setLock( &qa_lock );	
			
		if ( unanswered_question )
		{
			unsetLock( &qa_lock );
			usleep(1000);
		}
		else
		{
			printf("          Student[%d] asks a question (%d/%d)\n", student_number, current_question, num_questions );
			
			setCurrentId( student_number );
			
			QuestionStart();
			
			unsetLock( &qa_lock );
			
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
}

void setLock( omp_lock_t *lock )
{
	omp_set_lock( lock );
}

void unsetLock( omp_lock_t *lock )
{
	omp_unset_lock( lock );
}

//EnterOffice() is only called after locking the capacity_mutex
void EnterOffice()
{
	current_capacity--;
}

void LeaveOffice()
{
	//INCREASE OFFICE CAPACITY
	setLock( &capacity_lock );
	
	current_capacity++;
	
	unsetLock( &capacity_lock );
	
	
	//DECREASE REMAINING STUDENTS
	setLock( &students_lock );
	
	remaining_students--;

	unsetLock( &students_lock );
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
	setLock( &qa_lock );
	
	unanswered_question = 0;
	answer_given = 0;
	
	unsetLock( &qa_lock );
}

void ProfessorLoop()
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
	
	setLock( &qa_lock );
	
	answer_given = 1;
	
	unsetLock( &qa_lock );
}

//GETTER AND SETTER METHODS
int getCapacity()
{
	int val;

	setLock( &capacity_lock );
	
	val = current_capacity;

	unsetLock( &capacity_lock );
	
	return val;
}

int getRemainingStudents()
{
	int val;
	
	setLock( &students_lock );
	
	val = remaining_students;
	
	unsetLock( &students_lock );
	
	return val;
}

int getUnansweredQuestion()
{
	int val;
	
	setLock( &qa_lock );
	
	val = unanswered_question;
	
	unsetLock( &qa_lock );
	
	return val;
}

int getAnswerGiven()
{
	int val;

	setLock( &qa_lock );
	
	val = answer_given;
	
	unsetLock( &qa_lock );
	
	return val;
}

void setUnansweredQuestion()
{

	setLock( &qa_lock );

	if ( unanswered_question == 0 )
	{
		unanswered_question = 1;
	}
	else
	{
		unanswered_question = 0;
	}
	
	unsetLock( &qa_lock );
}

int getCurrentId()
{
	int val;
	
	setLock( &id_lock );
	
	val = current_id;
	
	unsetLock( &id_lock );
	
	return val;
}

void setCurrentId( int id )
{
	setLock( &id_lock );
	
	current_id = id;
	
	unsetLock( &id_lock );
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
