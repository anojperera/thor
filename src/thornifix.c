/* Implementation of common functions */
/* The current date is: 25/01/2013  */
#include <stdlib.h>
#include <string.h>
#include "thornifix.h"

int thor_interpol(const double* x, const double* y, int n, double* z, double* fz, int m)
{
    int i, j, k;
    double* _tbl, *_coef;
    double _t;

    /* allocate storage */
    _tbl = (double*) calloc(n, sizeof(double));
    if(_tbl == NULL)
	return 1;
    _coef = (double*) calloc(n, sizeof(double));
    if(_coef == NULL)
	{
	    free(_tbl);
	    return 1;
	}
    /* initialise coefficnets */
    for(i=0; i<n; i++)
	_tbl[i] = y[i];

    /* work out the coefficients of the interpolating polynomial */
    _coef[0] = _tbl[0];

    for(k=1; k<n; k++)
	{
	    for(i=0; i<n-k; i++)
		{
		    j=i+k;
		    _tbl[i] = (_tbl[i+1] - _tbl[i]) / (x[j] - x[i]);
		}
	    _coef[k] = _tbl[0];
	}
    free(_tbl);

    /* work out interpolating polynomial specified points */
    for(k=0; k<m; k++)
	{
	    fz[k] = _coef[0];
	    for(j=1; j<n; j++)
		{
		    _t = _coef[j];
		    for(i=0; i<j; i++)
			_t = _t * (z[k] - x[i]);

		    fz[k] += _t;
		}
	}
    free(_coef);
    return 0;
}

/* decode message */
int thornifix_decode_msg(const char* buff, size_t size, struct thor_msg* msg)
{
    int _cnt;
    char* _t_buff;
    char* _tok;
    char _msg_buff[THORNIFIX_MSG_ELM_NUM][THORNIFIX_MSG_BUFF_ELM_SZ];
    
    /* check for arguments */
    if(buff == NULL || msg == NULL)
	return -1;

    /* initialise buffer */
    thorinifix_init_msg(msg);

    /* copy message to a local buffer */
    _t_buff = (char*) malloc(sizeof(char)*(size+1));
    strncpy(_t_buff, buff, size);
    _t_buff[size] = '\0';

    /* initialise char message buffer */
    for(_cnt=0; _cnt<THORNIFIX_MSG_ELM_NUM; _cnt++)
	memset((void*) _msg_buff[_cnt], 0, THORNIFIX_MSG_BUFF_ELM_SZ);

    /* tokenize the string */
    _cnt = 0;
    _tok = strtok(_t_buff, "|");
    while(_tok != NULL)
	{
	    /* copy exact bytes to the char buffer */
	    strncpy(_msg_buff[_cnt], _tok, THORNIFIX_MSG_BUFF_ELM_SZ-1);
	    _msg_buff[_cnt][THORNIFIX_MSG_BUFF_ELM_SZ-1] = '\0';
	    _tok = strtok(NULL, "|");
	    if(++_cnt >= THORNIFIX_MSG_ELM_NUM)
		break;
	}

    /* copy to message buffer */
    msg->_cmd = _msg_buff[0][0] != '\0'? atoi(_msg_buff[0]) : 0;
    msg->_ai0_val = _msg_buff[1][0] != '\0'? atof(_msg_buff[1]) : 0;
    msg->_ai1_val = _msg_buff[2][0] != '\0'? atof(_msg_buff[2]) : 0;
    msg->_ao0_val = _msg_buff[3][0] != '\0'? atof(_msg_buff[3]) : 0;
    msg->_ao1_val = _msg_buff[4][0] != '\0'? atof(_msg_buff[4]) : 0;
    msg->_ao2_val = _msg_buff[5][0] != '\0'? atof(_msg_buff[5]) : 0;
    msg->_ao3_val = _msg_buff[6][0] != '\0'? atof(_msg_buff[6]) : 0;
    msg->_ao4_val = _msg_buff[7][0] != '\0'? atof(_msg_buff[7]) : 0;
    msg->_ao5_val = _msg_buff[8][0] != '\0'? atof(_msg_buff[8]) : 0;
    msg->_ao6_val = _msg_buff[9][0] != '\0'? atof(_msg_buff[9]) : 0;
    msg->_ao7_val = _msg_buff[10][0] != '\0'? atof(_msg_buff[10]) : 0;
    msg->_ao8_val = _msg_buff[11][0] != '\0'? atof(_msg_buff[11]) : 0;
    msg->_ao9_val = _msg_buff[12][0] != '\0'? atof(_msg_buff[12]) : 0;
    msg->_ao10_val = _msg_buff[13][0] != '\0'? atof(_msg_buff[13]) : 0;
    msg->_di0_val = _msg_buff[14][0] != '\0'? atof(_msg_buff[14]) : 0;
    msg->_di1_val = _msg_buff[15][0] != '\0'? atof(_msg_buff[15]) : 0;

    return 0;
}
