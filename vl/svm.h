/** @file svm.h
 ** @brief Common functions and structures for SVMs
 ** @author Milan Sulc
 ** @author Daniele Perrone
 ** @author Andrea Vedaldi
 **/

/*
Copyright (C) 2013 Milan Sulc.
Copyright (C) 2012 Daniele Perrone.
Copyright (C) 2011-13 Andrea Vedaldi.

All rights reserved.

This file is part of the VLFeat library and is made available under
the terms of the BSD license (see the COPYING file).
*/


/**
<!-- ------------------------------------------------------------- -->
@page svm Support Vector Machines (SVM)
@author Milan Sulc
@author Daniele Perrone
@author Andrea Vedaldi
@tableofcontents
<!-- ------------------------------------------------------------- -->

Support Vector Machines (SVMs) are one of the most popular types of
discriminative classifiers. VLFeat implements two large scale
solvers - SGC and (S)DCA - to learn linear SVMs. These linear solvers can
be combined with explicit feature maps to learn non-linear models too.
Variants of the standard SVM formulation are supported as well.

Please see @subpage svm-fundamentals for introduction to Support Vector Machines and their learning.
For further details please refer to:

- @subpage svm-advanced
- @subpage svm-sgd
- @subpage svm-sdca

<!-- ------------------------------------------------------------- -->
@section svm-starting Getting started
<!-- ------------------------------------------------------------- -->
The following example demonstrates learning an SVM.

@code

#include <stdio.h>
#include "vl/svm.h"
#include "vl/svm_sgd.h"
#include "vl/svm_dca.h"

int main()
{

  // lets have some data structure and build a dataset upon them
  double someData2D[8] = { 0, -0.6,   0, 0.6,   // x
                        -0.5, -0.3, 0.5,   0    // y
        };
  vl_size dimension = 2;
  VlSvmDataset * dataset = vl_svmdataset_new(someData2D,dimension) ;
  vl_int8 labels[4] = {1,1, -1, -1};
  vl_size numSamples = 4;

  // create inner product and accumulator functions for your data structure
  double innerProduct (const void* dataset, const vl_uindex element,
                       const double* model) {
    VlSvmDataset* sDataset = (VlSvmDataset*) dataset ;
    double* data  = (double*) sDataset->data ;
    return model[0]*data[element] + model[1]*data[element+sDataset->dimension] ;
  }

  void accumulator (const void* dataset, const vl_uindex element,
                    double * model, const double multiplier)
  {
    VlSvmDataset* sDataset = (VlSvmDataset*) dataset ;
    double* data  = (double*) sDataset->data ;

    model[0] += multiplier * data[element];
    model[1] += multiplier * data[element+sDataset->dimension];
  }


  // create a VlSvm structure for SGD
  double lambda = 0.01;
  VlSvm* svm_sgd = vl_svm_new(dimension, lambda, VL_SVM_SGD);

  // train SVM using the SGD method
  vl_svm_sgd_train (svm_sgd,dataset, numSamples,&innerProduct, &accumulator,labels) ;
  printf("SGD model w = [ %f , %f ] , bias b = %f \n",svm_sgd->model[0],svm_sgd->model[1],svm_sgd->bias);

  // now similiar for SDCA
  VlSvm* svm_dca = vl_svm_new(dimension, lambda, VL_SVM_DCA);
  vl_svm_dca_train(svm_dca,dataset,numSamples, &innerProduct, &accumulator, NULL, &vl_L1_loss, &vl_L1_lossConjugate, vl_L1_deltaAlpha, labels);
  printf("DCA model w = [ %f , %f ] , bias b = %f \n",svm_dca->model[0],svm_dca->model[1],svm_dca->bias);

  return 0;
}

@endcode
*/

/**
<!-- ------------------------------------------------------------- -->
@page svm-fundamentals SVM fundamentals
@tableofcontents
<!-- ------------------------------------------------------------- -->

Let's have a vector $ \bx \in \real^d $ representing, for example, an image, an
audio track, or a fragment of text. Our goal is to design a
*classifier*, i.e. a function that associates to each vector $\bx$ a
positive or negative label based on a desired criterion, for example
the fact that the image contains or not a cat, that the audio track
contains or not English speech, or that the text is or is not a CVPR
paper.

The vector $\bx$ is classified by looking at the sign of a *linear
scoring function* $\langle \bx, \bw \rangle$. The goal of learning is
to estimate the parameter $\bw \in \real^d$ in such a way that the
score is positive if the vector $\bx$ belongs to the positive class
and negative otherwise. In fact, in the standard SVM formulation the
the goal is to have a score of *at least 1* in the first case, and of
*at most -1* in the second one, imposing a *marging*.

The parameter $\bw$ is estimated or *learned* by fitting the scoring
function to a training set of $n$ example pairs $(\bx_i,y_i),
i=1,\dots,n$. Here $y_i \in \{-1,1\}$ are the *ground truth labels* of
the corresponding example vectors. The fit quality is measured by a
*loss function* which, in standard SVMs, is the *hinge loss*:

\[
   \ell_i(\langle \bw,\bx\rangle) = \max\{0, 1 - y_i \langle \bw,\bx\rangle\}.
\]

Note that the hinge loss is zero only if the score $\langle
\bw,\bx\rangle$ is in fact at least 1 or at most -1, depending on the
label $y_i$.

Fitting the training data is usually insufficent. In order for the
scoring function *generalize to future data* as well, it is usually
preferable to trade off the fitting accruacy with the *regularity* of
the learned function. Regularity in the standard formulation is
measured by the norm of the parameter vector $\|\bw\|^2$ (see @ref
svm-advanced). Averaging the loss for all examples and summing
regularizer weighed by a parameter $\lambda$ yields the *regularised
loss objective*

@f{equation}{
\boxed{\displaystyle
 E(\bw) =  \frac{\lambda}{2} \left\| \bw \right\|^2 + \frac{1}{n} \sum_{i=1}^n \max\{0, 1 - y_i \langle \bw,\bx\rangle\}.
 \label{e:svm-primal-hinge}
}
@f}

Note that this objective function is *convex*. Amont the many
benefits, the major one is that the objective has a single global
optimum.

@ref svm-learning shows how VLFeat can be used to learn an SVM by
minmizing $E(\bw)$.

<!-- ------------------------------------------------------------- -->
@section svm-learning Learning
<!-- ------------------------------------------------------------- -->

Learning an SVM amounts to finding the minimizer $\bw^*$ of the cost
functional $E(\bw)$. While there are dozens of methods that can be
used to do so, VLFeat implements two large scale methods, designed to
work with linear SVM (see @ref svm-feature-maps to go beyond linear):

- @ref svm-sgd
- @ref svm-sdca

To use the solvers, create a ::VlSvm object, set any parameters you
need to change, and call ::vl_svm_dca_train().

**Give a walkthrough example in detail**

So far SVMs have been linear and unbiased. @ref svm-bias discusses how
a bias term can be added to the SVM. @ref svm-feature-maps discusses
advanced features, including learning non-linear models by introducing
feature maps.

<!-- ------------------------------------------------------------- -->
@subsection svm-bias Learning a bias term
<!-- ------------------------------------------------------------- -->

It is common to add to the SVM scoring function a *bias term* $b$, and
to consider the scor $\langle \bx,\bw \rangle + b$. In practice the
bias term can be crucial to fit the training data optimally, as there
is no reason why the inner products $\langle \bx,\bw \rangle$ sould be
naturally centered around $+1$ and $-1$.

Some SVM learning diretctly estimateboth $\bw$ and $b$. However,
certain algorithm like SGD and SDCA do not. In this case, a common
approach is to simply add a constant $B > 0$ (we call it the *bias
multiplier*) to the data, i.e.
\[
 \bar \bx = \begin{bmatrix} \bx \\ B \end{bmatrix},
\quad
 \bar \bw = \begin{bmatrix} \bw \\ w_b \end{bmatrix}.
\]
In this manner the scoring function incorporates implicitly a bias $b = B w_b$:
\[
 \langle \bar\bx, \bar\bw \rangle =
 \langle \bx, \bw \rangle + B w_b.
\]

The disadvantage is that the term $w_b^2$ becomes part of the SVM
regularizer, which shrinks the bias $b$ towards zero. This effect can
be reduced by making $B$ sufficiently large, because in this case
$\|\bw\|^2 \gg w_b^2$ and the shrinking effect is negligible.

Unforunately, making $B$ too large increases numerically instability,
so a reasonalbe trade-off is generally sought. Specific
implementations of SGD and SDCA may provide explicit support to learn
the bias, but it is important to understand the implications on speed
and accuracy.

<!-- ------------------------------------------------------------- -->
@subsection svm-feature-maps Learning non-liner SVMs: feature maps
<!-- ------------------------------------------------------------- -->

So far the SVM scoring function $\langle \bx,\bw \rangle$ has been
presented as linear. However, its construction assumes that the
objects to be classified have been represented or encoded as suitable
vectors $\bx$. The representation can be made explicity by
introducting the *feature map* $\Phi(\bx) \in \real^d$. Including the
feature map yields a scoring function *non-linear* in $\bx$:

\[
\bx\in\mathcal{X} \quad\longrightarrow\quad \langle \Phi(\bx), \bw \rangle.
\]

Note, in fact, that the nature of the input space $\mathcal{X}$ can be
arbitrary and might not have a vector space structure at all.

The representation or encoding captures a notion of *similarity*
between objects: if two vectors $\Phi(\bx_1)$ and $\Phi(\bx_2)$ are
similar, then their scores will also be similar. Note that choosing a
feature map amounts to incorporating this information in the model
*prior* to learning.

The relation of feature maps to similarity functions is formalized by
the notion of a *kernel*, a positive definite function $K(\bx,\bx')$
measuring the similarity of a pair of objects. A feature map defines a
kernel by

\[
 K(\bx,\bx') = \langle \Phi(\bx),\Phi(\bx') \rangle.
\]

Viceversa, any kernel function can be represented by a feature map in
this manner, establishing an equivalence.

So far, all solvers in VLFeat assume that the feature map $\Psi(\bx)$
can be explicitly computed. Although classically kernels were
introduced to generalize solvers to non-linear SVMs for which a
feature map *cannot* be computed (e.g. for a Gaussian kernel the
feature map is infinite dimensional), in practice using explicit
feature representations yields much faster solvers.

*/


/**
<!-- ------------------------------------------------------------- -->
@page svm-advanced Advanced SVM topics
@tableofcontents
<!-- ------------------------------------------------------------- -->

This page dicsusses advanced SVM topics. For an introduction to SVMs,
please refer to @ref svm.

@section svm-loss-functions Loss functions

The basic SVM \eqref{e:svm-primal-hinge} is just one of several loss
functions that can be used when training an SVM. More in general, one
can cosider the objective

@f{equation}{
 E(\bw) =  \frac{\lambda}{2} \left\| \bw \right\|^2 + \frac{1}{n} \sum_{i=1}^n \ell_i(\langle \bw,\bx\rangle).
 \label{e:svm-primal}
@f}

where the loss $\ell_i(z)$ is a convex function of the scalar variable
$z$. Losses differ by: (i) their purpose (some are suitalbe for
classification, other for regression), (ii) their smoothness (which
usually affects how quickly the SVM objective function can be
minimized), and (iii) their statistical interpretation (for example
the logistic loss can be used to learn logistic models).

Concrete examples are the:

<table>
<tr>
<td>Name</td>
<td>Loss $\ell_i(z)$</td>
<td>Description</td>
</tr>
<tr>
<td>Hinge</td>
<td>$\max\{0, 1-y_i z\}$</td>
<td>The standard SVM loss function.</td>
</tr>
<tr>
<td>Square hinge</td>
<td>$\max\{0, 1-y_i z\}^2$</td>
<td>The standard SVM loss function, but squared. This version is
smoother and may yield numerically easier problems.</td>
</tr>
<tr>
<td>Square or l2</td>
<td>$(y_i - z)^2$</td>
<td>This loss yields the ridge regression model (l2 regularised least
square).</td>
</tr>
<tr>
<td>Linear or l1</td>
<td>$|y_i - z|$</td>
<td>Another loss suitable for regression, usually more robust but
harder to optmise than the squared one.</td>
</tr>
<tr>
<td>Insensitive l1</td>
<td>$\max\{0, |y_i - z| - \epsilon\}$.</td>
<td>This is a variant of the previous loss, proposed in the original
Support Vector Regression formulation. Differently from the previous
two losses, the insensitivity may yeld to a sparse selection of
support vectors (see @ref svm-support-vectors).</td>
</tr>
</table>

<!-- ------------------------------------------------------------- -->
@section svm-data-abstraction Data abstraction: working with compressed data
<!-- ------------------------------------------------------------- -->

VLFeat learning algorithms (SGD and SDCA) access the data by means of
only two operations:

- *inner product*: computing the inner product between the model and
  a data vector, i.e. $\langle \bw, \bx \rangle$.
- *accumulation*: summing a data vector to the model, i.e. $\bw
  \leftarrow \bw + \beta \bx$.

VLFeat learning algorithms are *parameterized* in these two
operations. As a consequence, the data can be stored in any format
suitable to the user (e.g. dense matrices, sparse matrices,
block-sparse matrices, disk caches, and so on) provided that these two
operations can be implemented efficiently. Differently from the data,
however, the model vector $\bw$ is represented simply as a dense array
of doubles. This choice is adequate in almost any case.

A particularly useful aspect of this design choice is that the
training data can be store in *compressed format* (for example by
using product quantization (PQ)). Furthermore, higher-dimensional
encodings such as the homogeneous kernel map (@ref homkermap) and the
intersection kernel map can be *computed on the fly*. Such techniques
are very important when dealing with GBs of data.

<!-- ------------------------------------------------------------- -->
@section svm-dual-problem Dual problem
<!-- ------------------------------------------------------------- -->

In optmisiation, the *dual objective* $D(\balpha)$ of the SVM
objective $E(\bw)$ is of great interest. To obtain the dual objective,
one starts by approximating each loss term from below by a family of planes:
\[
\ell_i(z) = \sup_{u} (u z - \ell_i^*(u) ),
\qquad
\ell_i^*(u) = \sup_{z} (z u - \ell_i(z) )
\]
where $\ell_i^*(u)$ is the *dual cojugate* of the loss and gives the
intercept of each approximating plane as a function of the slope. When
the loss function is convex, the approximation is in fact exact.

Since each plane $- z \alpha_i - \ell^*_i(-\alpha_i) \leq \ell_i(z)$
bounds the loss from below, by substituting in $E(\bw)$ one can write
a lower bound for the SVM objective
\[
F(\bw,\balpha) =  \frac{\lambda}{2} \|\bw\|^2 -
\frac{1}{n}\sum_{i=1}^n (\bw^\top \bx_i\alpha_i + \ell_i^*(-\alpha_i))
\leq E(\bw).
\]
for each setting of the *dual variables* $\alpha_i$. The dual
objective function $D(\balpha)$ is obtained by minimizing the lower
bound $F(\bw,\balpha)$ w.r.t. to $\bw$:
\[
 D(\balpha) = \inf_{\bw} F(\bw,\balpha) \leq E(\bw).
\]
The minimizer is and dual objective are now easy to find:
\[

\boxed{\displaystyle
\bw(\balpha) = \frac{1}{\lambda n} \sum_{i=1}^n \bx_i \alpha_i = \frac{1}{\lambda n} X\balpha,
\quad
 D(\balpha) = - \frac{1}{2\lambda n^2} \balpha^\top X^\top X \balpha +
\frac{1}{n} \sum_{i=1}^n - \ell_i^*(-\alpha_i)
}
\]
where $X = [\bx_1, \dots, \bx_n]$ is the data matrix. Since the dual is uniformly smaller than the primal, one has the *dualtiy gap* bound:
\[
 D(\balpha) \leq P(\bw^*) \leq P(\bw(\balpha))
\]
This bound can be used to evaluate how far off $\bw(\balpha)$ is from
the primal minimizer $\bw^*$. In fact, due to convexity, this
bound can be shown to be zero when $\balpha^*$ is the dual maximizer (strong duality):
\[
 D(\balpha^*) = P(\bw^*) = P(\bw(\balpha^*)),
\quad \bw^* = \bw(\balpha^*).
\]

<!-- ------------------------------------------------------------- -->
@section svm-C Parametrization in C
<!-- ------------------------------------------------------------- -->

Often a slightly different form of the SVM objective is considered,
where a parameter $C$ is used to scale the loss instead of the regulariser:

\[
E_C(\bw) = \frac{1}{2} \|\bw\|^2 + C \sum_{i=1}^n \ell_i(\langle \bx_i, \bw\rangle)
\]

This and the objecive function $E(\bw)$ in $\lambda$ are equivalent
(proportional) if

\[
 \lambda = \frac{1}{nC},
\qquad C = \frac{1}{n\lambda}.
\] up to an overall scaling factor to the problem.

**/

/**

<!-- ------------------------------------------------------------- -->
@page svm-sdca Stochastic Dual Coordinate Ascent
@tableofcontents
<!-- ------------------------------------------------------------- -->

This page describes the *Stochastic Dual Coordinate Ascent* (SDCA)
linear SVM solver. SDCA maximizes the dual SVM objective (see @ref
svm-dual-problem):

\[
 D(\balpha) = - \frac{1}{2\lambda n^2} \balpha^\top X^\top X \balpha +
\frac{1}{n} \sum_{i=1}^n - \ell_i^*(-\alpha_i)
\]

where $X$ is the data matrix. Recall that the primal parameter
corresponding to a given setting of the dual variables is:

\[
\bw(\balpha) = \frac{1}{\lambda n} \sum_{i=1}^n \bx_i \alpha_i = \frac{1}{\lambda n} X\balpha
\]

In its most basic form, the SDCA algorithm can be summarised as follows:

- Let $\balpha_0 = 0$.
- Until the duality gap $P(\bw(\balpha_t)) -  D(\balpha_t) < \epsilon$
  - Pick a dual variable $q$ uniformly at random in $1, \dots, n$.
  - Maximise the dual with respect to this variable:
    \[
      \Delta\balpha_q = \max_{\Delta\balpha_q} D(\balpha_t + \Delta\balpha_q e_q ).
    \]
  - Update $\alpha_{q,t+1} = \alpha_{q,t} + \Delta\balpha_q e_q $.

In VLFeat, we partially use the nomenclature from @cite{shwartz13a-dual} and @cite{Hsieh-2008-DCD}.

@section svm-sdca-dual-max Dual coordinate maximization

The updated dual objective can be expanded as:
\[
D(\balpha_t + e_q \Delta\balpha_q) =
\text{const.}
- \frac{1}{2\lambda n^2} \bx_q^\top \bx_q
- \frac{1}{n} \frac{X\balpha_t}{\lambda n} \bx_q^\top \Delta\balpha_q
- \frac{1}{n} \ell_q(- \alpha_q - \Delta\balpha_q)
\]
Maximising this quantity in the scalar variable $\Delta\balpha$ is usually a
simple and cheap affair. It is convenient to store and incrementally
update the model $\bw_t$ after the optimal step $\Delta\balpha$ has been
determined:
\[
 \bw_t = \frac{X \balpha_t}{\lambda n},
 \quad \bw_{t+1} = \bw_t + \frac{1}{\lambda n }\bx_q \Delta\balpha_q.
\]

For example, consider the hinge loss $\ell_q(z) = \max\{0, 1- y_qz \},
y_q \in \{-1,1\}$. The conjugate loss can be easily seen to be
\[
 \ell_q^*(u) =
\begin{cases}
y_q u, & -1 \leq y_q u \leq 0, \\
+\infty, & \text{otherwise}.
\end{cases}
\]


Plugging in and ignoring the domain bounds, the update can be obtained
by setting the derivate w.r.t $\Delta\balpha_q$ equal to zero, obtaining
\[
 \tilde {\Delta \balpha_q }= \frac{y_q - f}{k/(\lambda n)},
\quad f = \frac{\bx_q^\top X\balpha_t}{\lambda n} = \bx_q^\top \bw_t,
\quad k = \bx_q^\top \bx_q.
\]
Note that $f{}$ is simply the current score associated by the SVM to
sample $\bx_q$. Accounting for the fact that it must be $-1 \leq - y_q
\alpha_q \leq 0$, or $0 \leq y_q \alpha_q \leq 1$, one obtains the update
\[
 \Delta\balpha_q =  y_q \max\{0, \min\{1, \tilde {\Delta\balpha_q }+ y_q \alpha_q\}\} - \alpha_q.
\]

@section svm-sdca-features Implementation features


<!-- ------------------------------------------------------------ --->
@subsection svm-sdca-rand Random permutation of sub-problems
<!-- ------------------------------------------------------------ --->

The implementation of SDCA uses a random order of sub-problems at each iteration, which leads to faster training.
This means, that in each outer iteration, @f$ {1 , ..., m } @f$ is permuted to @f$ {\pi(1) , ..., \pi(m) } @f$
and the subpromlems are solved in the order of @f$ \alpha_{\pi(1)}, \alpha_{\pi(2)}, ... , \alpha_{\pi(m)} @f$

Still, there is an option to disable this feature and use a non-stochastic DCA method.

<!-- ------------------------------------------------------------ --->
@subsection svm-sdcd-online Online setting
<!-- ------------------------------------------------------------ --->
As a huge number of instances can cause an expensive outer iteration, it may be convenient to choose and update only one @f$ \alpha_i @f$
at the @f$ k @f$-th outer iteration.

In @cite{Hsieh-2008-DCD}, Online setting means randomly choosing an index @f$ i @f$ at a time.

However, in VLFeat implementation, the Online setting option means doing the following three steps in every inner iteration:
- run the diagnostic (if requested)
- increase the number of "outer" iterations done
- check the stopping condition (according to the preset frequency)

Thus, compared to the proposed Online setting, in VLFeat implementation, chosen indexes do not repeat before processing whole @f$ A @f$, where @f$ A = {1,..,n} @f$.

Online setting is an optional feature.

**/


/**
<!-- ------------------------------------------------------------- -->
@page svm-sgd Stochastic Gradient Descent
@tableofcontents
<!-- ------------------------------------------------------------- -->

This page describes the *Stochastic Graident Descent* (SGD) linear SVM
solver. SGD optimizes the primal SVM objective:

\[
  E(\bw) = \frac{\lambda}{2} \left\| \bw \right\|^2 + \frac{1}{n} \sum_{i=1}^n
 \ell_i(\langle \bw,\bx\rangle)
\]

The starting point is to rewrite the objective as the average

\[
E(\bw) = \frac{1}{n} \sum_{i=1}^n E_i(\bw),
\quad
E_i(\bw) = \frac{\lambda}{2}  \left\| \bw \right\|^2 + \ell_i(\langle \bw,\bx\rangle).
\]

Then SGD performs gradient steps by considering one term $E_i(\bw)$
selected at random. In its most basic form, the algorithm is
straightforward to describe:

- Start with $\bw_0 = 0$.
- For $t=1,2,\dots T$:
  - Sample one index $i$ in $1,\dots,n$ uniformly at random.
  - Compute a subgradient $\bg_t$ of $E_i(\bw)$ at $\bw_t$.
  - Compute the learning rate $\eta_t$.
  - Update $\bw_{t+1} = \bw_t - \eta_t \bg_t$.

Provided that the learning rate $\eta_t$ is choosen correctly, this
simple algorithm is guaranteed to converge to the minimizer $\bw^*$ of
$E$.

<!-- ------------------------------------------------------------- -->
@section svm-sgd-convergence Convergence and speed
<!-- ------------------------------------------------------------- -->

The goal of the SGD algorithm is to bring the *primal suboptimality*
below a threshold $\epsilon_P$:
\[
  E(\bw_t) - E(\bw^*) \leq \epsilon_P.
\]

If the learning rate $\eta_t$ is selected appropritely, SGD can be
shown to converge properly. For example,
@cite{shalev-shwartz07pegasos} show that, since $E(\bw)$ is
$\lambda$-strongly convex, then using the learning rate
\[
 \eta_t = \frac{1}{\lambda t}.
\]
guarantees that the algorithm reaches primal-subpotimality $\epsilon_P$ in
\[
 \tilde O\left( \frac{1}{\lambda \epsilon_P} \right).
\]
iterations. This particular SGD algorithm is known as PEGASOS.

The convergence speed is not sufficient to tell the *learning speed*,
i.e. how quickly an algorithm can learn an SVM that performs optimally
on the test set (the ultimate goal). The following two observations
can be used to link convergence speed to learning speed:

- The regularizer strength is often heuristically selected to be
  inversely proportional to the number of training samples: $\lambda =
  \lambda_0 /n$. This reflects the fact that with more training data
  the prior should count less.
- The primal suboptimality $\epsilon_P$ should be about the same as
  the estimation error of the SVM primal. This estimation error is due
  to the finite training set size and can be shown to be of the order
  of $1/\lambda n = 1 / \lambda_0$.

Under these two assumptions, PEGASOS can learn a linear SVM in time
$\tilde O(n)$, which is *linear in the number of training
examples*. This fares much better with $O(n^2)$ or worse of non-linear
SVM solvers.

<!-- ------------------------------------------------------------- -->
@section svm-sgd-bias The bias term
<!-- ------------------------------------------------------------- -->

As noted in @ref svm-bias, adding a bias term to the SVM is often
necessary. In SGD this is obtained by appending the bias multiplier
$B$ to the data vectors (@ref svm-bias). As noted, the bias multiplier
should be relatively large in order to avoid shrinking the bias
towards zero. Unfortunately, having a large bias multiplier makes SGD
unstable and slow.

The solution is, unforunately, a hack. The learning rate for the bias
component of the model is reduced by multiplying it further by a
`biasLearningRate` coefficient, usually selected to be inversely
proportional to the `biasMultiplier` parameter.

<!-- ------------------------------------------------------------ --->
@section svm-pegasos PEGASOS
<!-- ------------------------------------------------------------ --->

<!-- ------------------------------------------------------------ --->
@subsection svm-pegasos-algorithm Algorithm
<!-- ------------------------------------------------------------ --->

PEGASOS @cite{shalev-shwartz07pegasos} is a stochastic subgradient
optimizer. At the <em>t</em>-th iteration the algorithm:

- Samples uniformly at random as subset @f$ A_t @f$ of <em>k</em> of
  training pairs @f$(x,y)@f$ from the <em>m</em> pairs provided for
  training (this subset is called mini batch).
- Computes a subgradient @f$ \nabla_t @f$ of the function @f$ E_t(w) =
  \frac{1}{2}\|w\|^2 + \frac{1}{k} \sum_{(x,y) \in A_t} \ell(w;(x,y))
  @f$ (this is the SVM objective function restricted to the
  minibatch).
- Compute an intermediate weight vector @f$ w_{t+1/2} @f$ by doing a
  step @f$ w_{t+1/2} = w_t - \alpha_t \nalba_t @f$ with learning rate
  @f$ \alpha_t = 1/(\eta t) @f$ along the subgradient. Note that the
  learning rate is inversely proportional to the iteration numeber.
- Back projects the weight vector @f$ w_{t+1/2} @f$ on the
  hypersphere of radius @f$ \sqrt{\lambda} @f$ to obtain the next
  model estimate @f$ w_{t+1} @f$:
  @f[
     w_t = \min\{1, \sqrt{\lambda}/\|w\|\} w_{t+1/2}.
  @f]
  The hypersfere is guaranteed to contain the optimal weight vector
  @f$ w^* @f$.

VLFeat implementation fixes to one the size of the mini batches @f$ k
@f$.


<!-- ------------------------------------------------------------ --->
@subsection svm-pegasos-restarting Restarting
<!-- ------------------------------------------------------------ --->

VLFeat PEGASOS implementation can be restarted after any given number
of iterations. This is useful to compute intermediate statistics or to
load new data from disk for large datasets.  It sufficient to use the
Svm state object as input for the next run.
Notice that a model learned using a "stopped-restarted" solver could
slightly differ from one learned from a unique run of the solver. The
difference could be more noticable if the user provides a permutation
to decide the order of the data points visits.

<!-- ------------------------------------------------------------ --->
@subsection svm-pegasos-permutation Permutation
<!-- ------------------------------------------------------------ --->

VLFeat PEGASOS can use a user-defined permutation to decide the order
in which data points are visited (instead of using random
sampling). By specifying a permutation the algorithm is guaranteed to
visit each data point exactly once in each loop. The permutation needs
not to be bijective. This can be used to visit certain data samples
more or less often than others, implicitly reweighting their relative
importance in the SVM objective function. This can be used to blanace
the data.

<!-- ------------------------------------------------------------ --->
@subsection svm-pegasos-kernels Non-linear kernels
<!-- ------------------------------------------------------------ --->

PEGASOS can be extended to non-linear kernels, but the algorithm is
not particularly efficient in this setting [1]. When possible, it may
be preferable to work with explicit feature maps.

Let @f$ k(x,y) @f$ be a positive definite kernel. A <em>feature
map</em> is a function @f$ \Psi(x) @f$ such that @f$ k(x,y) = \langle
\Psi(x), \Psi(y) \rangle @f$. Using this representation the non-linear
SVM learning objective function writes:

@f[
 \min_{w} \frac{\lambda}{2} \|w\|^2 + \frac{1}{m} \sum_{i=1}^n
 \ell(w; (\Psi(x)_i,y_i)).
@f]

Thus the only difference with the linear case is that the feature @f$
\Psi(x) @f$ is used in place of the data @f$ x @f$.

@f$ \Psi(x) @f$ can be learned off-line, for instance by using the
incomplete Cholesky decomposition @f$ V^\top V @f$ of the Gram matrix
@f$ K = [k(x_i,x_j)] @f$ (in this case @f$ \Psi(x_i) @f$ is the
<em>i</em>-th columns of <em>V</em>). Alternatively, for additive
kernels (e.g. intersection, Chi2) the explicit feature map computed by
@ref homkermap.h can be used.

For additive kernels it is also possible to perform the feature
expansion online inside the solver, setting the specific feature map
via ::vl_svmdataset_set_map. This is particular useful to keep the
size of the training data small, when the number of the samples is big
or the memory is limited.

*/

#ifndef VL_SVMS_H
#define VL_SVMS_H

#include "svmdataset.h"


#define VL_SVM_SGD  1
#define	VL_SVM_DCA  2


/** @brief Binary Svm Objective Function
 **
 ** This structure keeps some statistics on a specific classifier
 ** learned with the SVM Solver.
 **/
typedef struct _VlSvmObjective {
  double energy ;               /**< full energy value. */
  double regularizer ;          /**< regularization term. */
  double lossPos ;              /**< energy term due to false positives. */
  double lossNeg ;              /**< energy term due to false negatives. */
  double hardLossPos ;          /**< number of false positives. */
  double hardLossNeg ;          /**< number of false negatives. */
  /* for DCA : */
  double lossDual ;            /**< sum of conjugate (-alpha) losses */
  double energyDual ;          /**< the value of SVM dual objective */
  double dualityGap ;          /**< duality gap = primal - dual */

} VlSvmObjective ;


/** @typedef VlSvmDiagnostics
 ** @brief Pointer to a function called for
 ** diagnostics purposes.
 **
 ** @param svm is an instance of @ref VlSvm .
 **
 **/
typedef void (*VlSvmDiagnostics) (void *svm) ;


/** @brief Svm Solver
 **
 ** This structure represents the status of an SVM solver.
 **/
typedef struct _VlSvm {
  vl_uint8 type;                /**< svm type - VL_SVM_SGD or VL_SVM_DCA. */
  double * model ;             /**< svm model. */
  double bias ;                 /**< bias element. */
  vl_size dimension ;           /**< model length. */
  vl_size iterations ;          /**< number of iterations. */
  vl_size maxIterations ;       /**< maximum number of iterations. */
  double epsilon ;              /**< stopping criterion threshold */
  double lambda ;               /**< regularization parameter. */
  double biasMultiplier ;       /**< bias strength multiplier. */
  double elapsedTime ;          /**< elapsed time from algorithm start. */
  VlSvmObjective*  objective ;  /**< value and statistics of the objective function. */
  VlSvmDiagnostics diagnostic ; /**< diagnostic MEX function. */
  void * diagnosticFunction ; /**< reference to diagnostic MATLAB function */
  //vl_size diagnosticFreq;     /**< number of iterations between diagnostics */

// SGD specific:
  vl_size energyFrequency ;     /**< frequency of computation of svm energy */
  double  biasPreconditioner ;  /**< bias learning preconditioner.  */
  VlRand* randomGenerator ;     /**< random generator.  */
  vl_uint32 * permutation ;     /**< data permutation. */
  vl_size permutationSize ;     /**< permutation size. */

// DCA specific:
  vl_bool randomPermutation; /**< use Random Permutation of Sub-problems */
  vl_bool onlineSetting; /**< use an Online Setting */
  double * alpha;

} VlSvm;






/** @typedef VlSvmLossFunction
 ** @brief Pointer to a function that defines the loss function
 **/
typedef double (*VlSvmLossFunction)(vl_int8 label, double inner) ;

/** @typedef VlSvmLossConjugateFunction
 ** @brief Pointer to a function that defines the conjugate function of the loss function
 **/
typedef double (*VlSvmLossConjugateFunction)(vl_int8 label, double alpha);

/** @typedef VlSvmDeltaAlpha
 ** @brief Pointer to a function that defines the computation of alpha addition
 **/
typedef double (*VlSvmDeltaAlpha)(vl_int8 label, double inner, VlSvm * svm, double xi_square, double alpha, vl_size numSamples);





/** ------------------------------------------------------------------
** @brief Create a new @ref VlSvm structure
** @param dimension svm model dimension.
** @param lambda regularization parameter.
** @param type SVM type ( VL_SVM_SGD / VL_SVM_DCA )
**
** This function allocates and returns a new @ref VlSvm structure.
**
** @return the new @ref VlSvm structure.
** @sa ::vl_svm_delete
**/
VL_INLINE
VlSvm* vl_svm_new (vl_size dimension, double lambda, vl_uint8 type) {

  VlSvm* svm = (VlSvm*) vl_malloc(sizeof(VlSvm)) ;

  svm->type = type;

  svm->model = (double*) vl_calloc(dimension, sizeof(double)) ;
  svm->dimension = dimension ;
  svm->objective = (VlSvmObjective*) vl_malloc(sizeof(VlSvmObjective)) ;
  svm->iterations = 0 ;
  svm->maxIterations = 1000;
  svm->lambda = lambda ;
  svm->epsilon = 0 ;
  svm->biasMultiplier = 1 ;
  svm->bias = 0 ;
  svm->elapsedTime = 0 ;

  svm->diagnostic = NULL ;
  //svm->diagnosticFreq = 100 ;
  svm->diagnosticFunction = NULL ;


  /* SGD specific: */
  svm->biasPreconditioner = 1 ;
  svm->energyFrequency = 100 ;
  svm->randomGenerator = NULL ;
  svm->permutation = NULL ;


  /* DCA specific: */
  svm->randomPermutation = 1;
  svm->onlineSetting = 0;


  return svm ;
}

/** -------------------------------------------------------------------
 ** @brief Delete a @ref VlSvm structure
 ** @param *svm @ref VlSvm structure.
 **
 ** The function frees the resources allocated by
 ** ::vl_svm_new().
 **/

VL_INLINE
void vl_svm_delete (VlSvm * svm, vl_bool freeModel)
{
  if (svm->model && freeModel) vl_free(svm->model) ;
  if (svm->objective) vl_free(svm->objective) ;
  vl_free(svm) ;
}


/** -------------------------------------------------------------------
 ** @brief Compute diagnostics for an @ref VlSvm structure
 ** @param *svm @ref VlSvm structure.
 ** @param *dataset pointer to training dataset
 ** @param numSamples number of training samples
 ** @param labels labels for training data (+1/-1)
 ** @param innerProduct function defining the innerProduct between the model and a data point.
 **/

VL_INLINE
void
vl_svm_compute_diagnostic(VlSvm *svm,
                          void * dataset,
                          vl_size numSamples,
                          vl_int8 const * labels,
                          VlSvmDatasetInnerProduct innerProduct,
                          VlSvmLossFunction lossFunction,
                          VlSvmLossConjugateFunction lossConjugateFunction
                          )
{
  vl_size i, k ;
  double inner, loss;

  svm->objective->regularizer = svm->bias * svm->bias;
  for (i = 0; i < svm->dimension; i++) {
    svm->objective->regularizer += svm->model[i] * svm->model[i] ;
  }
  svm->objective->regularizer *= svm->lambda * 0.5 ;

  svm->objective->lossPos = 0 ;
  svm->objective->lossNeg = 0 ;
  svm->objective->hardLossPos = 0 ;
  svm->objective->hardLossNeg = 0 ;

  svm->objective->lossDual = 0 ;



  for (k = 0; k < numSamples; k++) {

    inner = innerProduct(dataset,k,svm->model) ;
    inner += svm->bias * svm->biasMultiplier;
    loss = lossFunction(labels[k],inner) ;

    if (labels[k] < 0) {
      svm->objective->lossNeg += loss ;
      svm->objective->hardLossNeg += (loss > 0) ;
    } else {
      svm->objective->lossPos += loss ;
      svm->objective->hardLossPos += (loss > 0) ;
    }

    if (svm->type == VL_SVM_DCA) svm->objective->lossDual += lossConjugateFunction(labels[k], - svm->alpha[k]);
  }

  svm->objective->lossPos /= numSamples ;
  svm->objective->lossNeg /= numSamples ;
  svm->objective->hardLossPos /= numSamples ;
  svm->objective->hardLossNeg /= numSamples ;
  svm->objective->energy = svm->objective->regularizer + svm->objective->lossPos + svm->objective->lossNeg ;


  if (svm->type == VL_SVM_DCA) {
    svm->objective->lossDual /= numSamples ;
    svm->objective->energyDual = - svm->objective->regularizer - svm->objective->lossDual;
    svm->objective->dualityGap = svm->objective->energy - svm->objective->energyDual;
  }

}




/** @name Methods for different loss functions
 ** @{
 **/

VL_INLINE
double vl_L1_loss(vl_int8 label, double inner) {
  return VL_MAX(1 - label*inner, 0.0);
}

VL_INLINE
double vl_L2_loss(vl_int8 label, double inner) {
  double l = VL_MAX(1 - label*inner, 0.0);
  return l*l;
}

VL_INLINE
double vl_L1_lossConjugate (vl_int8 label, double alpha) {
  return label*alpha;
}

VL_INLINE
double vl_L2_lossConjugate (vl_int8 label, double alpha) {
  return (label+alpha/4)*alpha;
}

VL_INLINE
double vl_L1_deltaAlpha (vl_int8 label, double inner, VlSvm * svm, double xi_square, double alpha, vl_size numSamples) {
  //return svm->lambda*numSamples*(label-inner)/xi_square;
  return label * VL_MAX(0, VL_MIN(1,  svm->lambda*numSamples*(1-label*inner)/xi_square +alpha*label ) ) - alpha;
}

VL_INLINE
double vl_L2_deltaAlpha (vl_int8 label, double inner, VlSvm * svm, double xi_square, double alpha, vl_size numSamples) {
  //return svm->lambda*numSamples*(label-inner-alpha/2)/(xi_square + svm->lambda*numSamples/2);
  return label * VL_MAX(0, (svm->lambda*numSamples*(1-label*inner-label*alpha/2)/(xi_square + svm->lambda*numSamples/2) + alpha*label)) - alpha;

}



/* VL_SVMS_H */
#endif