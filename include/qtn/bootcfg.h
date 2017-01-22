/*
 * Copyright (c) 2009 Quantenna Communications, Inc.
 * All rights reserved.
 *
 *  Syscfg module - uses config sector for common filesytem between linux and
 *  uboot.
 */


typedef u32 bootcfg_t;

/******************************************************************************
	Function:   bootcfg_create
	Purpose:	create file
 	Returns:	0 if successful			
  	Note:  	    if size is zero, the proc entry is created but
  	            no data is allocated until the first write
 *****************************************************************************/
int bootcfg_create(const char *filename,u32 size);

/******************************************************************************
	Function:   bootcfg_delete
	Purpose:	delete file
 	Returns:	0 if successful			
  	Note:  	    
 *****************************************************************************/
int bootcfg_delete(const char *token);

/******************************************************************************
   Function:    bootcfg_get_var
   Purpose:     Get variable from environment
   Returns:     NULL if variable not found, pointer to storage otherwise
   Note:        variable value copied to storage
 *****************************************************************************/
char* bootcfg_get_var(const char *variable, char *storage);

/******************************************************************************
   Function:    bootcfg_set_var
   Purpose:     Set variable to environment
   Returns:     NULL if variable not found, pointer to storage otherwise
   Note:        variable value copied to storage
 *****************************************************************************/
int bootcfg_set_var(const char *var, const char *value);

