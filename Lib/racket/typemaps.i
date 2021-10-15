
%typemap(rktin) SWIGTYPE *OUTPUT "[$1_name : (_ptr o $typemap(rktin,$1_*type))]";
%typemap(rktargout) SWIGTYPE *OUTPUT "$1_name";

%typemap(rktin) SWIGTYPE *INOUT "[$1_name : (_ptr io $typemap(rktin,$1_*type))]";
%typemap(rktargout) SWIGTYPE *INOUT "$1_name";
