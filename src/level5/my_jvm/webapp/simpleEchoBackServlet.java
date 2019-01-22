import java.io.IOException;
import java.io.OutputStream;
import java.io.PrintWriter;

import javax.servlet.Servlet;
import javax.servlet.ServletConfig;
import javax.servlet.ServletException;
import javax.servlet.ServletRequest;
import javax.servlet.ServletResponse;

/**
 * 
 */

/**
 * @author user1
 *
 */
public class simpleEchoBackServlet implements Servlet {

	@Override
	public void destroy() {
		// TODO Auto-generated method stub
		
	}

	@Override
	public ServletConfig getServletConfig() {
		// TODO Auto-generated method stub
		return null;
	}

	@Override
	public String getServletInfo() {
		// TODO Auto-generated method stub
		return null;
	}

	@Override
	public void init(ServletConfig conf) throws ServletException {
		// TODO Auto-generated method stub		
	}

	@Override
	public void service(ServletRequest req, ServletResponse rsp) {

		String strResp = "this's " + this.getClass().getName() ;

		
		try {
			PrintWriter writer = rsp.getWriter() ;
			String rawResp = "HTTP/1.1 " + "200 Ok" + "\n" + 
	                "Server: Apache-Coyote/1.1\r\n" +
			        "Content-Type: text/html;charset=GBK\n" +
	                "Content-Length: " + strResp.length() +
	                "\r\n\r\n" + 
	                strResp;
			
			
			writer.write(rawResp);
			writer.flush();
		}
		catch (Exception e) {
			System.out.println("send response fail: " + e.getMessage());
		}
	}

}
