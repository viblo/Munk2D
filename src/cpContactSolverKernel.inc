/* Instantiated for vector and scalar arithmetic; keep operation order identical. */
typedef struct {V x,y,w,bx,by,bw;} Velocity;
static inline Velocity getVelocity(cpContactSolverContext *p,const int *ids){
 Velocity b;
#ifdef PACKET_SCALAR
 const double *v=p->velocity+6*ids[0];b.x=v[0];b.y=v[1];b.w=v[2];b.bx=v[3];b.by=v[4];b.bw=v[5];
#else
 getAoS(p->velocity,ids,&b.x,&b.y,&b.w,&b.bx,&b.by,&b.bw);
#endif
 return b;
}
static inline void putVelocity(cpContactSolverContext *p,const int *ids,int lanes,Velocity b){
#ifdef PACKET_SCALAR
 (void)lanes;
 double *v=p->velocity+6*ids[0];v[0]=b.x;v[1]=b.y;v[2]=b.w;v[3]=b.bx;v[4]=b.by;v[5]=b.bw;
#else
 putAoS(p->velocity,ids,lanes,b.x,b.y,b.w,b.bx,b.by,b.bw);
#endif
}
static inline V dot(V ax,V ay,V bx,V by){return add(mul(ax,bx),mul(ay,by));}
static inline void impulse(Velocity *v,V m,V i,V rx,V ry,V jx,V jy,V jbx,V jby){
 v->bx=add(v->bx,mul(jbx,m));v->by=add(v->by,mul(jby,m));v->bw=add(v->bw,mul(i,sub(mul(rx,jby),mul(ry,jbx))));
 v->x=add(v->x,mul(jx,m));v->y=add(v->y,mul(jy,m));v->w=add(v->w,mul(i,sub(mul(rx,jy),mul(ry,jx))));
}
static void solvePacket(cpContactSolverContext *p,cpContactSolverPacket *q,int lane){
 Velocity a=getVelocity(p,q->a+lane),b=getVelocity(p,q->b+lane);
 V nx=load(q->nx),ny=load(q->ny),sx=load(q->sx),sy=load(q->sy),friction=load(q->u);
 for(int k=0;k<q->count;k++){
  cpContactSolverContact *c=&q->con[k];V r1x=load(c->r1x),r1y=load(c->r1y),r2x=load(c->r2x),r2y=load(c->r2y);
  V vb1x=add(a.bx,mul(neg(r1y),a.bw)),vb1y=add(a.by,mul(r1x,a.bw));
  V vb2x=add(b.bx,mul(neg(r2y),b.bw)),vb2y=add(b.by,mul(r2x,b.bw));
  V vrx=add(sub(add(b.x,mul(neg(r2y),b.w)),add(a.x,mul(neg(r1y),a.w))),sx);
  V vry=add(sub(add(b.y,mul(r2x,b.w)),add(a.y,mul(r1x,a.w))),sy);
  V vbn=dot(sub(vb2x,vb1x),sub(vb2y,vb1y),nx,ny),vrn=dot(vrx,vry,nx,ny);
  V nMass=load(c->nMass),oldBias=load(c->jBias),oldN=load(c->jnAcc);
  V jBias=vmax(add(oldBias,mul(sub(load(c->bias),vbn),nMass)),zero());
  V jn=vmax(add(oldN,mul(neg(add(load(c->bounce),vrn)),nMass)),zero());
  store(c->jBias,jBias);store(c->jnAcc,jn);
  V db=sub(jBias,oldBias),dn=sub(jn,oldN),jbx=mul(nx,db),jby=mul(ny,db),jx,jy;
  if(q->friction){
   V vrt=dot(vrx,vry,neg(ny),nx),limit=mul(friction,jn),oldT=load(c->jtAcc);
   V jt=vmin(vmax(add(oldT,mul(neg(vrt),load(c->tMass))),neg(limit)),limit);store(c->jtAcc,jt);
   V dt=sub(jt,oldT);jx=sub(mul(nx,dn),mul(ny,dt));jy=add(mul(nx,dt),mul(ny,dn));
  }else{jx=mul(nx,dn);jy=mul(ny,dn);}
  if(q->mode!=1)impulse(&a,load(q->am),load(q->ai),r1x,r1y,neg(jx),neg(jy),neg(jbx),neg(jby));
  if(q->mode!=2)impulse(&b,load(q->bm),load(q->bi),r2x,r2y,jx,jy,jbx,jby);
 }
 if(q->mode!=1)putVelocity(p,q->a+lane,q->lanes,a);if(q->mode!=2)putVelocity(p,q->b+lane,q->lanes,b);
}
