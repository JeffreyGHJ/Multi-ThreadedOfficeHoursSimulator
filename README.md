# Multi-ThreadedOfficeHoursSimulator
Simulates students entering a professor's office. The office capacity is limited and other students may not enter the office until the students inside the office have had their questions answered and have left the office to make space for more.

office_hours.c (POSIX thread variant):
	
		compile:			gcc office_hours.c -o office_hours -lpthread
		run: 				./office_hours [num_students] [office_capacity]
  
omp_office.c	(OpenMP variant):		
	
		compile:			gcc -fopenmp omp_office.c -o omp_office
		run:				./mp_office [num_students] [office_capacity]
