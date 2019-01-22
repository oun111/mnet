import java.io.BufferedReader;
import java.io.File;
import java.io.FileNotFoundException;
import java.io.FileReader;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.io.PrintWriter;
import java.io.UnsupportedEncodingException;
import java.net.ServerSocket;
import java.net.Socket;
import java.net.URL;
import java.net.URLClassLoader;
import java.net.URLStreamHandler;
import java.util.Enumeration;
import java.util.Locale;
import java.util.Map;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;

import javax.servlet.AsyncContext;
import javax.servlet.DispatcherType;
import javax.servlet.RequestDispatcher;
import javax.servlet.Servlet;
import javax.servlet.ServletConfig;
import javax.servlet.ServletContext;
import javax.servlet.ServletException;
import javax.servlet.ServletInputStream;
import javax.servlet.ServletOutputStream;
import javax.servlet.ServletRequest;
import javax.servlet.ServletResponse;

/**
 * 
 */

class smallRequest implements ServletRequest {
	
	private String method ;
	
	private String url ;
	
	
	
	smallRequest(String req) {
        String msgs[] = req.split("\n");
        
        
        setUrl(msgs[0].split("\\s")[1]);
        setMethod(msgs[0].split("\\s")[0]);
	}

	boolean isMethodPost() {
		return "post".equalsIgnoreCase(getMethod());
	}
	

	/**
	 * @return the method
	 */
	public final String getMethod() {
		return method;
	}



	/**
	 * @param method the method to set
	 */
	public final void setMethod(String method) {
		this.method = method;
	}



	/**
	 * @return the url
	 */
	public final String getUrl() {
		return url;
	}



	/**
	 * @param url the url to set
	 */
	public final void setUrl(String url) {
		this.url = url;
	}

	@Override
	public AsyncContext getAsyncContext() {
		// TODO Auto-generated method stub
		return null;
	}

	@Override
	public Object getAttribute(String arg0) {
		// TODO Auto-generated method stub
		return null;
	}

	@Override
	public Enumeration<String> getAttributeNames() {
		// TODO Auto-generated method stub
		return null;
	}

	@Override
	public String getCharacterEncoding() {
		// TODO Auto-generated method stub
		return null;
	}

	@Override
	public int getContentLength() {
		// TODO Auto-generated method stub
		return 0;
	}

	@Override
	public String getContentType() {
		// TODO Auto-generated method stub
		return null;
	}

	@Override
	public DispatcherType getDispatcherType() {
		// TODO Auto-generated method stub
		return null;
	}

	@Override
	public ServletInputStream getInputStream() throws IOException {
		// TODO Auto-generated method stub
		return null;
	}

	@Override
	public String getLocalAddr() {
		// TODO Auto-generated method stub
		return null;
	}

	@Override
	public String getLocalName() {
		// TODO Auto-generated method stub
		return null;
	}

	@Override
	public int getLocalPort() {
		// TODO Auto-generated method stub
		return 0;
	}

	@Override
	public Locale getLocale() {
		// TODO Auto-generated method stub
		return null;
	}

	@Override
	public Enumeration<Locale> getLocales() {
		// TODO Auto-generated method stub
		return null;
	}

	@Override
	public String getParameter(String arg0) {
		// TODO Auto-generated method stub
		return null;
	}

	@Override
	public Map<String, String[]> getParameterMap() {
		// TODO Auto-generated method stub
		return null;
	}

	@Override
	public Enumeration<String> getParameterNames() {
		// TODO Auto-generated method stub
		return null;
	}

	@Override
	public String[] getParameterValues(String arg0) {
		// TODO Auto-generated method stub
		return null;
	}

	@Override
	public String getProtocol() {
		// TODO Auto-generated method stub
		return null;
	}

	@Override
	public BufferedReader getReader() throws IOException {
		// TODO Auto-generated method stub
		return null;
	}

	@Override
	public String getRealPath(String arg0) {
		// TODO Auto-generated method stub
		return null;
	}

	@Override
	public String getRemoteAddr() {
		// TODO Auto-generated method stub
		return null;
	}

	@Override
	public String getRemoteHost() {
		// TODO Auto-generated method stub
		return null;
	}

	@Override
	public int getRemotePort() {
		// TODO Auto-generated method stub
		return 0;
	}

	@Override
	public RequestDispatcher getRequestDispatcher(String arg0) {
		// TODO Auto-generated method stub
		return null;
	}

	@Override
	public String getScheme() {
		// TODO Auto-generated method stub
		return null;
	}

	@Override
	public String getServerName() {
		// TODO Auto-generated method stub
		return null;
	}

	@Override
	public int getServerPort() {
		// TODO Auto-generated method stub
		return 0;
	}

	@Override
	public ServletContext getServletContext() {
		// TODO Auto-generated method stub
		return null;
	}

	@Override
	public boolean isAsyncStarted() {
		// TODO Auto-generated method stub
		return false;
	}

	@Override
	public boolean isAsyncSupported() {
		// TODO Auto-generated method stub
		return false;
	}

	@Override
	public boolean isSecure() {
		// TODO Auto-generated method stub
		return false;
	}

	@Override
	public void removeAttribute(String arg0) {
		// TODO Auto-generated method stub
		
	}

	@Override
	public void setAttribute(String arg0, Object arg1) {
		// TODO Auto-generated method stub
		
	}

	@Override
	public void setCharacterEncoding(String arg0) throws UnsupportedEncodingException {
		// TODO Auto-generated method stub
		
	}

	@Override
	public AsyncContext startAsync() {
		// TODO Auto-generated method stub
		return null;
	}

	@Override
	public AsyncContext startAsync(ServletRequest arg0, ServletResponse arg1) {
		// TODO Auto-generated method stub
		return null;
	}
}


class smallResponse {
	
	private String url ;
	
	private String method ;
	
	
	smallResponse(String url, String method) {
		this.setUrl(url);
		this.setMethod(method);
	}
	
	final String getFileList() {
		
		String body = "<html>" + 
	              " <head>" +
	              "  <title> " +
			      "  client request: " +
	              "  </title> " +
	              " </head>" +
			      " <body>" ;
	
		// search for files
		File file = new File(".");
		File[] fileList = file.listFiles();
		
		for (File f : fileList) {
			body += "<li><a href=\"" + f.getAbsolutePath() + "\">" +
				    f.getName() + "</a></li>";
		}
		
		body += " </body> </html>\r\n" ;
	
		String rawRsp = "HTTP/1.1 200 OK\n" + 
		                "Server: Apache-Coyote/1.1\r\n" +
				        "Content-Type: text/html;charset=GBK\n" +
		                "Content-Length: " + body.length() +
		                "\r\n\r\n" + 
		                body;
	
		return rawRsp ;	
	}
	
	final String getFile() throws Exception {
		String content = "" ;
		String state = "200 Ok";

		try {
			File f = new File(this.getUrl());
			FileReader fr = new FileReader(f);
			char cb[] = new char[1024];
			
			while (fr.read(cb,0,1024)>0) {
				content += String.valueOf(cb) ;
			}
			
			fr.close();

		} catch (Exception e) {
			content = "read '" + this.getUrl() + "' error: " + e.getCause() ;
			state   = "404 Not Found";
		}
		
		String rawResp = "HTTP/1.1 " + state + "\n" + 
                "Server: Apache-Coyote/1.1\r\n" +
		        "Content-Type: text/html;charset=GBK\n" +
                "Content-Length: " + content.length() +
                "\r\n\r\n" + 
                content;
		
		return rawResp ;
	}
	
	public final String get() throws Exception {

		if (this.getUrl().equals("/")) {
			return getFileList();
		}

		return getFile() ;
	}

	/**
	 * @return the url
	 */
	public final String getUrl() {
		return url;
	}

	/**
	 * @param url the url to set
	 */
	public final void setUrl(String url) {
		this.url = url;
	}

	/**
	 * @return the method
	 */
	public final String getMethod() {
		return method;
	}

	/**
	 * @param method the method to set
	 */
	public final void setMethod(String method) {
		this.method = method;
	}

}

class smallServletResponse implements ServletResponse {
	
	PrintWriter m_writer ;
	OutputStream m_out ;

	
	smallServletResponse(OutputStream ostr) throws Exception {
		m_out = ostr ;
		m_writer = new PrintWriter(ostr,false);
	}
	
	@Override
	public void flushBuffer() throws IOException {
		// TODO Auto-generated method stub
		
	}

	@Override
	public int getBufferSize() {
		// TODO Auto-generated method stub
		return 0;
	}

	@Override
	public String getCharacterEncoding() {
		// TODO Auto-generated method stub
		return null;
	}

	@Override
	public String getContentType() {
		// TODO Auto-generated method stub
		return null;
	}

	@Override
	public Locale getLocale() {
		// TODO Auto-generated method stub
		return null;
	}

	@Override
	public ServletOutputStream getOutputStream() throws IOException {
		return (ServletOutputStream) m_out;
	}

	@Override
	public PrintWriter getWriter() throws IOException {
		return m_writer ;
	}

	@Override
	public boolean isCommitted() {
		// TODO Auto-generated method stub
		return false;
	}

	@Override
	public void reset() {
		// TODO Auto-generated method stub
		
	}

	@Override
	public void resetBuffer() {
		// TODO Auto-generated method stub
		
	}

	@Override
	public void setBufferSize(int arg0) {
		// TODO Auto-generated method stub
		
	}

	@Override
	public void setCharacterEncoding(String arg0) {
		// TODO Auto-generated method stub
		
	}

	@Override
	public void setContentLength(int arg0) {
		// TODO Auto-generated method stub
		
	}

	@Override
	public void setContentType(String arg0) {
		// TODO Auto-generated method stub
		
	}

	@Override
	public void setLocale(Locale arg0) {
		// TODO Auto-generated method stub
		
	}

}

abstract class smallWebSvrBase implements Runnable {

	Socket clientSock = null ;
	
	
	smallWebSvrBase(Socket clientSock) {
		this.clientSock = clientSock ;
		
        /*System.out.printf("client %s:%d connecting...\n",
        		clientSock.getInetAddress().getHostAddress(),
        		clientSock.getPort());*/
	}
	
	private String methodName() {
		String className = this.getClass().getName();
		String method = Thread.currentThread().getStackTrace()[4].getMethodName();
		
		return className + "." + method + "(): ";
	}
	
    private String rx() throws IOException {
    	
        InputStream istr = clientSock.getInputStream();
        byte[] data = new byte[1024];
        int ret = 0;

        
        ret = istr.read(data, 0, 1024) ;
        if (ret<=0)
        	return null ;
        
        //System.out.println("raw req: " + new String(data));
        //System.out.println(methodName() + " raw req " + ret + " bytes <<<<<<<<<<");

        return new String(data);
    }
    
    private void servletProcess(smallRequest req) throws Exception {
    	
    	String url = req.getUrl();
    	smallServletResponse rsp = new smallServletResponse(clientSock.getOutputStream());
    	String sname = url.substring(url.lastIndexOf("/")+1);
        URLClassLoader loader = null;
        Class<?> myClass = null;
        Servlet servlet = null;
        final String servletPath = "/mnt/sda5/zyw/work/mp2/src/level5/my_jvm/webapp/" ;

        
    	//System.out.println("servlet is " + sname);
    	
        try {
            URLStreamHandler streamHandler = null;
            //创建类加载器
            loader = new URLClassLoader(new URL[]{new URL(null, "file:" + 
                                        servletPath, streamHandler)});

            //加载对应的servlet类
            myClass = loader.loadClass(sname);
            
            //生产servlet实例
            servlet = (Servlet) myClass.newInstance();
            
            //执行ervlet的service方法
            servlet.service((ServletRequest)req,(ServletResponse)rsp);

        } catch (Exception e) {
            System.out.println(e.toString());
        }

    }
    
    private void staticProcess(String url, String method) throws Exception {
    	smallResponse rsp = new smallResponse(url,method);
    	String rawRsp = rsp.get() ;
    	OutputStream ostr = clientSock.getOutputStream();
    	
    	
    	//System.out.println("raw rsp: " + rawRsp);
    	//System.out.println(methodName() + " raw rsp " + rawRsp.length() + " bytes >>>>>>>>>>");
    	
    	ostr.write(rawRsp.getBytes());
    }
    
    private void tx(smallRequest req) throws Exception {
    	
    	String url = req.getUrl();
    	String method = req.getMethod();
    	OutputStream ostr = clientSock.getOutputStream();

    	/* 
    	 * servlet request
    	 */
    	if (url.toLowerCase().contains("servlet")) {
    		servletProcess(req);
    		return ;
    	}
    	
    	/*
    	 * static request
    	 */
    	staticProcess(url,method);
    }


	@Override
	public void run() {

    	try {
    		
    		while (isRunning()) {
    			
		        String rawReq = this.rx();
		        
		        // client disconnect
		        if (rawReq==null) {
		        	if (clientSock.isConnected())
		        		clientSock.close();
		        	//System.out.println("worker thread exit ...........");
		        	break ;
		        }
		        
		        smallRequest req = new smallRequest(rawReq);
		        
		        /*
		        Class<?> cls = req.getClass() ;
		        Class<?>[] intf= cls.getInterfaces() ;
		        for (Class<?> i : intf) {
		        	System.out.println("interface is: " + i.getName());
		        }
		        */
		        /*System.out.printf("method '%s' request '%s'\n",
		        		          req.getMethod(),req.getUrl());*/
		
		        // feed back
		    	this.tx(req);
    		}
	    	
		} catch (Exception e) {
            if (clientSock.isConnected())
				try {
					clientSock.close();
				} catch (IOException e1) {
					// TODO Auto-generated catch block
					e1.printStackTrace();
				}

			// TODO Auto-generated catch block
			e.printStackTrace();
		}

	}
	
	abstract boolean isRunning() ;
}

/**
 * @author user1
 *
 */
class smallCachedThreadPoolWebSvrImpl extends smallWebSvrBase {
	
	smallCachedThreadPoolWebSvrImpl(Socket clientSock) {
		super(clientSock);
	}

	@Override
	boolean isRunning() {
		return true;
	}
}


/**
 * @author user1
 *
 */
class smallThreadedWebSvrImpl extends smallWebSvrBase {
	
	private Thread t = null;
	
	
	smallThreadedWebSvrImpl(Socket clientSock) {
		super(clientSock);
	}

	public void start() {
		if (t==null) {
			t = new Thread(this,"thread") ;
			t.start(); 
		}
	}
	
	public void stop() {
		if (t!=null) {
			t.interrupt();
		}
	}

	@Override
	boolean isRunning() {
		return t.isInterrupted()==false;
	}

}


public class smallWebSvr {
	
	/**
	 * @param args
	 * @throws IOException 
	 */
	public static void main(String[] args) throws IOException {
		
    	ServerSocket serverSocket = null ;
    	ExecutorService cachedThreadPool = null;


        try {
            serverSocket=new ServerSocket(8881);
            serverSocket.setReuseAddress(true);
            
            cachedThreadPool = Executors.newCachedThreadPool();
            
        } catch (Exception e) {
            e.printStackTrace();
        }

        while (true) 
        {

          try {
			Socket socket = serverSocket.accept();
			
			// 自建线程方式：
			//smallThreadedWebSvrImpl svr = new smallThreadedWebSvrImpl(socket) ;
			//svr.start();

			// 线程池方式
			cachedThreadPool.execute(new smallCachedThreadPoolWebSvrImpl(socket));

          }  catch (Exception e) {
            e.printStackTrace();
          }

        }
        
	}
}
