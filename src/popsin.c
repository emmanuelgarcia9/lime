/*
 *  popsin.c
 *  This file is part of LIME, the versatile line modeling engine
 *
 *  Copyright (C) 2006-2014 Christian Brinch
 *  Copyright (C) 2015-2017 The LIME development team
 *
***TODO:
	- Change the definition of the file format so that nmol is now written with the other mol[] scalars.
 */

#include "lime.h"


void
popsin(configInfo *par, struct grid **gp, molData **md, int *popsdone){
  FILE *fp;
  int i,j,k;
  double dummy;
  struct cell *dc=NULL; /* Not used at present. */
  unsigned long numCells,nExtraSinks;

  if((fp=fopen(par->restart, "rb"))==NULL){
    if(!silent) bail_out("Error reading binary output populations file!");
    exit(1);
  }

  par->numDensities = 1;
  fread(&par->radius,   sizeof(double), 1, fp);
  fread(&par->ncell,    sizeof(int), 1, fp);
  fread(&par->nSpecies, sizeof(int), 1, fp);
  if( par->nSpecies < 0 || par->nSpecies > MAX_NSPECIES )
    {
      if(!silent) bail_out("Error reading binary output populations file : is this really a binary output file generated by lime ?");
      exit(1);
    }

  *md=realloc(*md, sizeof(molData)*par->nSpecies);

  for(i=0;i<par->nSpecies;i++){
    sprintf((*md)[i].molName, "unknown");
    (*md)[i].amass = -1.0;

    (*md)[i].eterm = NULL;
    (*md)[i].gstat = NULL;
    (*md)[i].cmb = NULL;
    fread(&(*md)[i].nlev,  sizeof(int),        1,fp);
    fread(&(*md)[i].nline, sizeof(int),        1,fp);
    fread(&(*md)[i].npart, sizeof(int),        1,fp);
    (*md)[i].part = malloc(sizeof(*((*md)[i].part))*(*md)[i].npart);
    for(j=0;j<(*md)[i].npart;j++){
      setCollPartsDefaults(&((*md)[i].part[j]));
      fread(&(*md)[i].part[j].ntrans, sizeof(int), 1,fp);
    }
    (*md)[i].lal=malloc(sizeof(int)*(*md)[i].nline);
    for(j=0;j<(*md)[i].nline;j++) fread(&(*md)[i].lal[j],    sizeof(int), 1,fp);
    (*md)[i].lau=malloc(sizeof(int)*(*md)[i].nline);
    for(j=0;j<(*md)[i].nline;j++) fread(&(*md)[i].lau[j],    sizeof(int), 1,fp);
    (*md)[i].aeinst=malloc(sizeof(double)*(*md)[i].nline);
    for(j=0;j<(*md)[i].nline;j++) fread(&(*md)[i].aeinst[j], sizeof(double), 1,fp);
    (*md)[i].freq=malloc(sizeof(double)*(*md)[i].nline);
    for(j=0;j<(*md)[i].nline;j++) fread(&(*md)[i].freq[j],   sizeof(double), 1,fp);
    (*md)[i].beinstl=malloc(sizeof(double)*(*md)[i].nline);
    for(j=0;j<(*md)[i].nline;j++) fread(&(*md)[i].beinstl[j],sizeof(double), 1,fp);
    (*md)[i].beinstu=malloc(sizeof(double)*(*md)[i].nline);
    for(j=0;j<(*md)[i].nline;j++) fread(&(*md)[i].beinstu[j],sizeof(double), 1,fp);
    for(j=0;j<(*md)[i].nline;j++) fread(&dummy,sizeof(double), 1,fp);
    fread(&dummy, sizeof(double),      1,fp);
    fread(&dummy, sizeof(double),   1,fp);
  }

  for(i=0;i<par->ncell;i++){
    fread(&(*gp)[i].id, sizeof (*gp)[i].id, 1, fp);
    fread(&(*gp)[i].x, sizeof (*gp)[i].x, 1, fp);
    fread(&(*gp)[i].vel, sizeof (*gp)[i].vel, 1, fp);
    fread(&(*gp)[i].sink, sizeof (*gp)[i].sink, 1, fp);
    for(j=0;j<par->nSpecies;j++)
      fread(&(*gp)[i].mol[j].nmol, sizeof(double), 1, fp);
    fread(&(*gp)[i].dopb_turb, sizeof (*gp)[i].dopb_turb, 1, fp);
    for(j=0;j<par->nSpecies;j++){
      (*gp)[i].mol[j].pops=malloc(sizeof(double)*(*md)[j].nlev);
      for(k=0;k<(*md)[j].nlev;k++) fread(&(*gp)[i].mol[j].pops[k], sizeof(double), 1, fp);
      for(k=0;k<(*md)[j].nline;k++) fread(&dummy, sizeof(double), 1, fp); /* knu */
      for(k=0;k<(*md)[j].nline;k++) fread(&dummy, sizeof(double), 1, fp); /* dust */
      fread(&(*gp)[i].mol[j].dopb,sizeof(double), 1, fp);
      fread(&(*gp)[i].mol[j].binv,sizeof(double), 1, fp);
    }
    fread(&dummy, sizeof(double), 1, fp);
    fread(&dummy, sizeof(double), 1, fp);
    fread(&dummy, sizeof(double), 1, fp);
  }
  fclose(fp);

  delaunay(DIM, *gp, (unsigned long)par->ncell, 0, 1, &dc, &numCells);

  /* We just asked delaunay() to flag any grid points with IDs lower than par->pIntensity (which means their distances from model centre are less than the model radius) but which are nevertheless found to be sink points by virtue of the geometry of the mesh of Delaunay cells. Now we need to reshuffle the list of grid points, then reset par->pIntensity, such that all the non-sink points still have IDs lower than par->pIntensity.
  */ 
  nExtraSinks = reorderGrid((unsigned long)par->ncell, *gp);
  par->pIntensity -= nExtraSinks;
  par->sinkPoints += nExtraSinks;

  distCalc(par, *gp);
  getVelocities(par,*gp);

  par->dataFlags |= (1 << DS_bit_x);
  par->dataFlags |= (1 << DS_bit_neighbours);
  par->dataFlags |= (1 << DS_bit_velocity);
  par->dataFlags |= (1 << DS_bit_abundance);
  par->dataFlags |= (1 << DS_bit_turb_doppler);
/*  par->dataFlags |= (1 << DS_bit_magfield); commented out because we are not yet reading it in popsin (and may never do so) */
  par->dataFlags |= (1 << DS_bit_ACOEFF);
  par->dataFlags |= (1 << DS_bit_populations);

  if(bitIsSet(defaultFuncFlags, FUNC_BIT_density)){
    if(!silent) bail_out("You need to supply a density() function.");
exit(1);
  }
  if(bitIsSet(defaultFuncFlags, FUNC_BIT_temperature)){
    if(!silent) bail_out("You need to supply a temperature() function.");
exit(1);
  }

  for(i=0;i<par->ncell; i++)
    (*gp)[i].dens = malloc(sizeof(double)*par->numDensities);
  for(i=0;i<par->pIntensity;i++)
    density((*gp)[i].x[0],(*gp)[i].x[1],(*gp)[i].x[2],(*gp)[i].dens);
  for(i=par->pIntensity;i<par->ncell;i++){
    for(j=0;j<par->numDensities;j++)
      (*gp)[i].dens[j]=1e-30;//************** what is the low but non zero value for?
  }

  par->dataFlags |= DS_mask_density;

  for(i=0;i<par->pIntensity;i++)
    temperature((*gp)[i].x[0],(*gp)[i].x[1],(*gp)[i].x[2],(*gp)[i].t);
  for(i=par->pIntensity;i<par->ncell;i++){
    (*gp)[i].t[0]=par->tcmb;
    (*gp)[i].t[1]=par->tcmb;
  }

  par->dataFlags |= DS_mask_temperatures;

  *popsdone=1;
  par->useAbun = 0;

  free(dc);
}

