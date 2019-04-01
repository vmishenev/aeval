#if !defined __AEVAL_EXCEPTION_HPP
#define __AEVAL_EXCEPTION_HPP

namespace ae
{
    class AeValException : public std::exception
    {
    protected:
        const std::string &ExceptionInfo;
    public:
        AeValException(const std::string &ExceptionInfo) :
            ExceptionInfo(ExceptionInfo)
        {
            //nothing
        }

		AeValException(const AeValException &Other) throw()
        : std::exception(Other), ExceptionInfo(Other.ExceptionInfo)
    	{
        // Nothing here
    	}

        virtual const char *what() const throw ()
        {
            return ExceptionInfo.c_str();
        }
        
    };

    class UnimplementedException : public AeValException
    {
    public:
        UnimplementedException(const std::string &ExceptionInfo) :
            AeValException(ExceptionInfo)
        {
            //nothing
        }

    };
} /* End namespace */

#endif
